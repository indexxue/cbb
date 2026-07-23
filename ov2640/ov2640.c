#include "ov2640.h"

#include <string.h>

#include "esp_cam_ctlr.h"
#include "esp_cam_ctlr_dvp.h"
#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_sccb_i2c.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ov2640";

#define OV2640_SCCB_FREQ_HZ (100000U)
/** XCLK 起来后 SCCB 就绪等待；过短时探测偶发超时。 */
#define OV2640_XCLK_SETTLE_MS (200U)
/** 等 0x30 ACK 的轮询次数 / 间隔。 */
#define OV2640_SCCB_PROBE_TRIES (10U)
#define OV2640_SCCB_PROBE_GAP_MS (50U)
/** detect / 读 PID 失败后的重试。 */
#define OV2640_DETECT_TRIES (5U)
#define OV2640_DETECT_GAP_MS (100U)
/** 置 1 时在 detect 前做全总线扫描（调试用，会刷大量 timeout 日志）。 */
#ifndef OV2640_I2C_FULL_SCAN
#define OV2640_I2C_FULL_SCAN (0)
#endif
#define OV2640_DEFAULT_RGB565_FMT "DVP_8bit_20Minput_RGB565_BE_240x240_25fps"
#define OV2640_DEFAULT_YUV422_FMT "DVP_8bit_20Minput_YUV422_240x240_25fps"

typedef struct {
    ov2640_t                *dev;
    esp_cam_ctlr_handle_t    cam;
    esp_cam_sensor_device_t *sensor;
    esp_sccb_io_handle_t     sccb;
    i2c_master_bus_handle_t  i2c_bus;
    bool                     i2c_bus_owned;
    void                    *frame_buf;
    size_t                   frame_buf_len;
    ov2640_frame_cb_t        frame_cb;
    void                    *frame_user;
    esp_cam_ctlr_trans_t     trans;
} ov2640_runtime_t;

static ov2640_runtime_t s_rt = {0};

static bool ov2640_gpio_valid(int pin)
{
    return (pin >= 0);
}

static gpio_num_t ov2640_gpio_or_nc(int pin)
{
    return ov2640_gpio_valid(pin) ? (gpio_num_t)pin : GPIO_NUM_NC;
}

static const char *ov2640_pick_format_name(const ov2640_config_t *cfg)
{
    if ((cfg->sensor_format_name != NULL) && (cfg->sensor_format_name[0] != '\0')) {
        return cfg->sensor_format_name;
    }
    if (cfg->format == OV2640_FMT_YUV422) {
        return OV2640_DEFAULT_YUV422_FMT;
    }
    return OV2640_DEFAULT_RGB565_FMT;
}

static ov2640_status_t ov2640_init_i2c_bus(const ov2640_config_t *cfg, i2c_master_bus_handle_t *bus_out, bool *owned_out)
{
    i2c_master_bus_handle_t bus = NULL;

    if ((bus_out == NULL) || (owned_out == NULL)) {
        return OV2640_ERROR_PARAM;
    }

    *owned_out = false;
    if (cfg->i2c_bus_handle != NULL) {
        *bus_out = (i2c_master_bus_handle_t)cfg->i2c_bus_handle;
        return OV2640_OK;
    }

    if ((cfg->i2c_port < 0) || (cfg->i2c_sda < 0) || (cfg->i2c_scl < 0)) {
        return OV2640_ERROR_PARAM;
    }

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = (i2c_port_num_t)cfg->i2c_port,
        .sda_io_num = (gpio_num_t)cfg->i2c_sda,
        .scl_io_num = (gpio_num_t)cfg->i2c_scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .flags.enable_internal_pullup = true,
    };

    if (i2c_new_master_bus(&bus_cfg, &bus) != ESP_OK) {
        return OV2640_ERROR_SCCB;
    }

    *bus_out = bus;
    *owned_out = true;
    return OV2640_OK;
}

#if OV2640_I2C_FULL_SCAN
static void ov2640_i2c_scan_bus(i2c_master_bus_handle_t bus, const char *stage)
{
    uint16_t found = 0U;

    ESP_LOGI(TAG, "I2C scan [%s]: SCL/SDA bus ready, range 0x08..0x77", stage);
    for (uint16_t addr = 0x08U; addr <= 0x77U; addr++) {
        if (i2c_master_probe(bus, (uint16_t)addr, 200) == ESP_OK) {
            ESP_LOGI(TAG, "I2C scan [%s]: ACK 0x%02X%s",
                     stage,
                     (unsigned)addr,
                     (addr == OV2640_SCCB_ADDR_7BIT_DEFAULT) ? " (OV2640)" : "");
            found++;
        }
    }
    if (found == 0U) {
        ESP_LOGW(TAG, "I2C scan [%s]: no device (check SCL/SDA wiring, pull-ups, module power)", stage);
    } else {
        ESP_LOGI(TAG, "I2C scan [%s]: total %u device(s)", stage, (unsigned)found);
    }
}
#endif

/**
 * 等 OV2640 SCCB（默认 0x30）给出 ACK。
 * 比全总线扫描更稳：不把空地址超时打脏总线，并覆盖上电/XCLK 就绪抖动。
 */
static bool ov2640_wait_sccb_ack(i2c_master_bus_handle_t bus, uint8_t addr7)
{
    for (uint32_t i = 0U; i < OV2640_SCCB_PROBE_TRIES; i++) {
        if (i2c_master_probe(bus, (uint16_t)addr7, 200) == ESP_OK) {
            if (i > 0U) {
                ESP_LOGI(TAG, "SCCB 0x%02X ACK after %u try(s)", (unsigned)addr7, (unsigned)(i + 1U));
            } else {
                ESP_LOGI(TAG, "SCCB 0x%02X ACK", (unsigned)addr7);
            }
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(OV2640_SCCB_PROBE_GAP_MS));
    }
    ESP_LOGW(TAG, "SCCB 0x%02X no ACK after %u tries (check SCL/SDA/XCLK/pull-ups)",
             (unsigned)addr7,
             (unsigned)OV2640_SCCB_PROBE_TRIES);
    return false;
}

static ov2640_status_t ov2640_init_sensor(const ov2640_config_t *cfg,
                                            i2c_master_bus_handle_t bus,
                                            esp_cam_sensor_device_t **sensor_out,
                                            esp_sccb_io_handle_t *sccb_out)
{
    esp_cam_sensor_config_t cam_cfg = {
        .reset_pin = ov2640_gpio_or_nc(cfg->pins.pin_reset),
        .pwdn_pin = ov2640_gpio_or_nc(cfg->pins.pin_pwdn),
        .xclk_pin = GPIO_NUM_NC,
    };
    esp_cam_sensor_device_t *sensor = NULL;
    const char *fmt_name = ov2640_pick_format_name(cfg);

    for (esp_cam_sensor_detect_fn_t *p = &__esp_cam_sensor_detect_fn_array_start; p < &__esp_cam_sensor_detect_fn_array_end;
         ++p) {
        sccb_i2c_config_t sccb_cfg = {
            .scl_speed_hz = OV2640_SCCB_FREQ_HZ,
            .device_address = p->sccb_addr,
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        };

        if (sccb_new_i2c_io(bus, &sccb_cfg, &cam_cfg.sccb_handle) != ESP_OK) {
            continue;
        }

        cam_cfg.sensor_port = p->port;
        sensor = p->detect(&cam_cfg);
        if (sensor != NULL) {
            if (p->port != ESP_CAM_SENSOR_DVP) {
                (void)esp_sccb_del_i2c_io(cam_cfg.sccb_handle);
                sensor = NULL;
                continue;
            }
            break;
        }
        (void)esp_sccb_del_i2c_io(cam_cfg.sccb_handle);
        cam_cfg.sccb_handle = NULL;
    }

    if (sensor == NULL) {
        ESP_LOGE(TAG, "OV2640 detect failed");
        return OV2640_ERROR_SCCB;
    }

    esp_cam_sensor_format_array_t fmt_array = {0};
    if (esp_cam_sensor_query_format(sensor, &fmt_array) != ESP_OK) {
        return OV2640_ERROR_SCCB;
    }

    const esp_cam_sensor_format_t *picked = NULL;
    for (int i = 0; i < fmt_array.count; i++) {
        if (strcmp(fmt_array.format_array[i].name, fmt_name) == 0) {
            picked = &fmt_array.format_array[i];
            break;
        }
    }
    if (picked == NULL) {
        ESP_LOGE(TAG, "format not found: %s (available %d):", fmt_name, fmt_array.count);
        for (int i = 0; i < fmt_array.count; i++) {
            ESP_LOGE(TAG, "  fmt[%d]: %s", i, fmt_array.format_array[i].name);
        }
        return OV2640_ERROR_SCCB;
    }

    if (esp_cam_sensor_set_format(sensor, picked) != ESP_OK) {
        ESP_LOGE(TAG, "set format failed");
        return OV2640_ERROR_SCCB;
    }

    ESP_LOGI(TAG, "sensor format: %s", picked->name);
    *sensor_out = sensor;
    *sccb_out = cam_cfg.sccb_handle;
    return OV2640_OK;
}

static bool ov2640_on_get_new_trans(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data)
{
    ov2640_runtime_t *rt = (ov2640_runtime_t *)user_data;
    (void)handle;
    *trans = rt->trans;
    return false;
}

static bool ov2640_on_trans_finished(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data)
{
    ov2640_runtime_t *rt = (ov2640_runtime_t *)user_data;
    (void)handle;
    if ((rt->frame_cb != NULL) && (rt->dev != NULL) && (trans->buffer != NULL)) {
        rt->frame_cb(rt->frame_user,
                     (const uint8_t *)trans->buffer,
                     rt->dev->frame_width,
                     rt->dev->frame_height);
    }
    return false;
}

bool ov2640_pins_valid(const ov2640_pin_config_t *pins)
{
    if (pins == NULL) {
        return false;
    }
    if ((pins->pin_pclk < 0) || (pins->pin_vsync < 0) || (pins->pin_href < 0)) {
        return false;
    }
    if ((pins->pin_d0 < 0) || (pins->pin_d1 < 0) || (pins->pin_d2 < 0) || (pins->pin_d3 < 0) || (pins->pin_d4 < 0) ||
        (pins->pin_d5 < 0) || (pins->pin_d6 < 0) || (pins->pin_d7 < 0)) {
        return false;
    }
    return true;
}

static bool ov2640_cfg_valid(const ov2640_config_t *cfg)
{
    if ((cfg == NULL) || (cfg->frame_width == 0U) || (cfg->frame_height == 0U)) {
        return false;
    }
    return ov2640_pins_valid(&cfg->pins);
}

ov2640_status_t ov2640_init_with_config(ov2640_t *dev, const ov2640_config_t *cfg)
{
    esp_cam_ctlr_dvp_pin_config_t pin_cfg = {0};
    esp_cam_ctlr_dvp_config_t dvp_cfg = {0};
    esp_cam_ctlr_evt_cbs_t cbs = {0};
    uint32_t buf_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA;
    size_t frame_bytes = 0U;

    if ((dev == NULL) || !ov2640_cfg_valid(cfg)) {
        return OV2640_ERROR_PARAM;
    }
    if (s_rt.dev != NULL) {
        return OV2640_ERROR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    memset(&s_rt, 0, sizeof(s_rt));

    dev->sccb_write = cfg->sccb_write;
    dev->sccb_read = cfg->sccb_read;
    dev->sccb_write_read = cfg->sccb_write_read;
    dev->delay_ms = cfg->delay_ms;
    dev->set_pwdn = cfg->set_pwdn;
    dev->set_reset = cfg->set_reset;
    dev->sccb_addr7 = (cfg->sccb_addr7 != 0U) ? cfg->sccb_addr7 : OV2640_SCCB_ADDR_7BIT_DEFAULT;
    dev->pins = cfg->pins;
    dev->frame_width = cfg->frame_width;
    dev->frame_height = cfg->frame_height;
    dev->format = cfg->format;

    if (ov2640_init_i2c_bus(cfg, &s_rt.i2c_bus, &s_rt.i2c_bus_owned) != OV2640_OK) {
        return OV2640_ERROR_SCCB;
    }

    pin_cfg.data_width = CAM_CTLR_DATA_WIDTH_8;
    pin_cfg.data_io[0] = (gpio_num_t)cfg->pins.pin_d0;
    pin_cfg.data_io[1] = (gpio_num_t)cfg->pins.pin_d1;
    pin_cfg.data_io[2] = (gpio_num_t)cfg->pins.pin_d2;
    pin_cfg.data_io[3] = (gpio_num_t)cfg->pins.pin_d3;
    pin_cfg.data_io[4] = (gpio_num_t)cfg->pins.pin_d4;
    pin_cfg.data_io[5] = (gpio_num_t)cfg->pins.pin_d5;
    pin_cfg.data_io[6] = (gpio_num_t)cfg->pins.pin_d6;
    pin_cfg.data_io[7] = (gpio_num_t)cfg->pins.pin_d7;
    pin_cfg.vsync_io = (gpio_num_t)cfg->pins.pin_vsync;
    pin_cfg.de_io = (gpio_num_t)cfg->pins.pin_href;
    pin_cfg.pclk_io = (gpio_num_t)cfg->pins.pin_pclk;
    pin_cfg.xclk_io = ov2640_gpio_valid(cfg->pins.pin_xclk) ? (gpio_num_t)cfg->pins.pin_xclk : GPIO_NUM_NC;

    dvp_cfg.ctlr_id = 0;
    dvp_cfg.clk_src = CAM_CLK_SRC_DEFAULT;
    dvp_cfg.h_res = cfg->frame_width;
    dvp_cfg.v_res = cfg->frame_height;
    dvp_cfg.input_data_color_type = (cfg->format == OV2640_FMT_YUV422) ? CAM_CTLR_COLOR_YUV422 : CAM_CTLR_COLOR_RGB565;
    dvp_cfg.dma_burst_size = 64;
    dvp_cfg.bk_buffer_dis = 1;
    dvp_cfg.pin = &pin_cfg;
    dvp_cfg.xclk_freq = (cfg->pins.xclk_hz != 0U) ? cfg->pins.xclk_hz : 20000000U;

    if (esp_cam_new_dvp_ctlr(&dvp_cfg, &s_rt.cam) != ESP_OK) {
        ESP_LOGE(TAG, "DVP controller init failed");
        if (s_rt.i2c_bus_owned) {
            (void)i2c_del_master_bus(s_rt.i2c_bus);
        }
        return OV2640_ERROR_CAPTURE;
    }

    /* OV2640 SCCB 需 XCLK 输出后才应答；DVP 创建时会启动 XCLK。 */
    vTaskDelay(pdMS_TO_TICKS(OV2640_XCLK_SETTLE_MS));

#if OV2640_I2C_FULL_SCAN
    ov2640_i2c_scan_bus(s_rt.i2c_bus, "post-XCLK");
#endif

    (void)ov2640_wait_sccb_ack(s_rt.i2c_bus, dev->sccb_addr7);

    {
        ov2640_status_t det = OV2640_ERROR_SCCB;
        for (uint32_t try = 0U; try < OV2640_DETECT_TRIES; try++) {
            det = ov2640_init_sensor(cfg, s_rt.i2c_bus, &s_rt.sensor, &s_rt.sccb);
            if (det == OV2640_OK) {
                if (try > 0U) {
                    ESP_LOGI(TAG, "detect OK on try %u/%u",
                             (unsigned)(try + 1U),
                             (unsigned)OV2640_DETECT_TRIES);
                }
                break;
            }
            ESP_LOGW(TAG, "detect try %u/%u failed, retry",
                     (unsigned)(try + 1U),
                     (unsigned)OV2640_DETECT_TRIES);
            (void)ov2640_wait_sccb_ack(s_rt.i2c_bus, dev->sccb_addr7);
            vTaskDelay(pdMS_TO_TICKS(OV2640_DETECT_GAP_MS));
        }
        if (det != OV2640_OK) {
            (void)esp_cam_ctlr_del(s_rt.cam);
            if (s_rt.i2c_bus_owned) {
                (void)i2c_del_master_bus(s_rt.i2c_bus);
            }
            return OV2640_ERROR_SCCB;
        }
    }

    frame_bytes = (size_t)cfg->frame_width * (size_t)cfg->frame_height * 2U;
    s_rt.frame_buf = esp_cam_ctlr_alloc_buffer(s_rt.cam, frame_bytes, buf_caps);
    if (s_rt.frame_buf == NULL) {
        ESP_LOGE(TAG, "frame buffer alloc %u failed", (unsigned)frame_bytes);
        (void)esp_cam_ctlr_del(s_rt.cam);
        (void)esp_sccb_del_i2c_io(s_rt.sccb);
        if (s_rt.i2c_bus_owned) {
            (void)i2c_del_master_bus(s_rt.i2c_bus);
        }
        return OV2640_ERROR_CAPTURE;
    }

    s_rt.frame_buf_len = frame_bytes;
    s_rt.trans.buffer = s_rt.frame_buf;
    s_rt.trans.buflen = frame_bytes;
    s_rt.dev = dev;

    cbs.on_get_new_trans = ov2640_on_get_new_trans;
    cbs.on_trans_finished = ov2640_on_trans_finished;
    if (esp_cam_ctlr_register_event_callbacks(s_rt.cam, &cbs, &s_rt) != ESP_OK) {
        (void)esp_cam_ctlr_del(s_rt.cam);
        (void)esp_sccb_del_i2c_io(s_rt.sccb);
        if (s_rt.i2c_bus_owned) {
            (void)i2c_del_master_bus(s_rt.i2c_bus);
        }
        return OV2640_ERROR_CAPTURE;
    }

    if (esp_cam_ctlr_enable(s_rt.cam) != ESP_OK) {
        (void)esp_cam_ctlr_del(s_rt.cam);
        (void)esp_sccb_del_i2c_io(s_rt.sccb);
        if (s_rt.i2c_bus_owned) {
            (void)i2c_del_master_bus(s_rt.i2c_bus);
        }
        return OV2640_ERROR_CAPTURE;
    }

#if 0
    if (cfg->format == OV2640_FMT_YUV422) {
        const cam_ctlr_format_conv_config_t conv_cfg = {
            .src_format = CAM_CTLR_COLOR_YUV422,
            .dst_format = CAM_CTLR_COLOR_RGB565,
            .conv_std = COLOR_CONV_STD_RGB_YUV_BT601,
            .data_width = 8,
            .input_range = COLOR_RANGE_LIMIT,
            .output_range = COLOR_RANGE_LIMIT,
        };
        (void)esp_cam_ctlr_format_conversion(s_rt.cam, &conv_cfg);
    }
#endif

    dev->initialized = true;
    ESP_LOGI(TAG, "ready %ux%u", (unsigned)dev->frame_width, (unsigned)dev->frame_height);
    return OV2640_OK;
}

ov2640_status_t ov2640_set_frame_callback(ov2640_t *dev, ov2640_frame_cb_t cb, void *user_ctx)
{
    if ((dev == NULL) || !dev->initialized || (s_rt.dev != dev)) {
        return OV2640_ERROR_NOT_INIT;
    }
    s_rt.frame_cb = cb;
    s_rt.frame_user = user_ctx;
    return OV2640_OK;
}

ov2640_status_t ov2640_start_stream(ov2640_t *dev)
{
    int enable = 1;

    if ((dev == NULL) || !dev->initialized || (s_rt.dev != dev)) {
        return OV2640_ERROR_NOT_INIT;
    }
    if (s_rt.sensor != NULL) {
        if (esp_cam_sensor_ioctl(s_rt.sensor, ESP_CAM_SENSOR_IOC_S_STREAM, &enable) != ESP_OK) {
            return OV2640_ERROR_CAPTURE;
        }
    }
    if (esp_cam_ctlr_start(s_rt.cam) != ESP_OK) {
        return OV2640_ERROR_CAPTURE;
    }
    dev->streaming = true;
    return OV2640_OK;
}

ov2640_status_t ov2640_stop_stream(ov2640_t *dev)
{
    int enable = 0;

    if ((dev == NULL) || !dev->initialized || (s_rt.dev != dev)) {
        return OV2640_ERROR_NOT_INIT;
    }
    (void)esp_cam_ctlr_stop(s_rt.cam);
    if (s_rt.sensor != NULL) {
        (void)esp_cam_sensor_ioctl(s_rt.sensor, ESP_CAM_SENSOR_IOC_S_STREAM, &enable);
    }
    dev->streaming = false;
    return OV2640_OK;
}

ov2640_status_t ov2640_capture_jpeg(ov2640_t *dev, uint8_t *buf, uint32_t buf_cap, uint32_t *out_len)
{
    (void)dev;
    (void)buf;
    (void)buf_cap;
    if (out_len != NULL) {
        *out_len = 0U;
    }
    return OV2640_ERROR_CAPTURE;
}

bool ov2640_is_initialized(const ov2640_t *dev)
{
    return (dev != NULL) && dev->initialized;
}
