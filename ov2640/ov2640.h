/**
 * @file ov2640.h
 * @brief OV2640 DVP 摄像头（esp_driver_cam + esp_cam_sensor）。
 */

#ifndef OV2640_H
#define OV2640_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OV2640_SCCB_ADDR_7BIT_DEFAULT (0x30U)

typedef int (*ov2640_sccb_write_t)(uint8_t addr7, const uint8_t *data, uint16_t len);
typedef int (*ov2640_sccb_read_t)(uint8_t addr7, uint8_t *data, uint16_t len);
typedef int (*ov2640_sccb_write_read_t)(uint8_t addr7,
                                          const uint8_t *write_data,
                                          uint16_t write_len,
                                          uint8_t *read_data,
                                          uint16_t read_len);
typedef void (*ov2640_delay_ms_t)(uint32_t ms);
typedef void (*ov2640_pin_t)(int level);

typedef enum {
    OV2640_OK = 0,
    OV2640_ERROR_PARAM,
    OV2640_ERROR_NOT_INIT,
    OV2640_ERROR_SCCB,
    OV2640_ERROR_CAPTURE,
} ov2640_status_t;

typedef enum {
    OV2640_FMT_RGB565 = 0,
    OV2640_FMT_JPEG,
    OV2640_FMT_YUV422,
} ov2640_pixel_format_t;

typedef struct {
    int pin_xclk;
    int pin_pclk;
    int pin_vsync;
    int pin_href;
    int pin_d0;
    int pin_d1;
    int pin_d2;
    int pin_d3;
    int pin_d4;
    int pin_d5;
    int pin_d6;
    int pin_d7;
    int pin_pwdn;
    int pin_reset;
    uint32_t xclk_hz;
} ov2640_pin_config_t;

typedef void (*ov2640_frame_cb_t)(void *user_ctx, const uint8_t *rgb565, uint16_t width, uint16_t height);

typedef struct {
    ov2640_sccb_write_t      sccb_write;
    ov2640_sccb_read_t       sccb_read;
    ov2640_sccb_write_read_t sccb_write_read;
    ov2640_delay_ms_t        delay_ms;
    ov2640_pin_t             set_pwdn;
    ov2640_pin_t             set_reset;
    uint8_t                  sccb_addr7;
    ov2640_pin_config_t      pins;
    uint16_t                 frame_width;
    uint16_t                 frame_height;
    ov2640_pixel_format_t    format;
    /** 可选：BoardInit 已创建的 I2C master 总线（i2c_master_bus_handle_t）。 */
    void                    *i2c_bus_handle;
    int8_t                   i2c_port;
    int8_t                   i2c_sda;
    int8_t                   i2c_scl;
    /** esp_cam_sensor 格式名；NULL 时按 RGB565 240x240 默认。 */
    const char              *sensor_format_name;
} ov2640_config_t;

typedef struct {
    ov2640_sccb_write_t      sccb_write;
    ov2640_sccb_read_t       sccb_read;
    ov2640_sccb_write_read_t sccb_write_read;
    ov2640_delay_ms_t        delay_ms;
    ov2640_pin_t             set_pwdn;
    ov2640_pin_t             set_reset;
    uint8_t                  sccb_addr7;
    ov2640_pin_config_t      pins;
    uint16_t                 frame_width;
    uint16_t                 frame_height;
    ov2640_pixel_format_t    format;
    bool                     initialized;
    bool                     streaming;
} ov2640_t;

ov2640_status_t ov2640_init_with_config(ov2640_t *dev, const ov2640_config_t *cfg);
ov2640_status_t ov2640_start_stream(ov2640_t *dev);
ov2640_status_t ov2640_stop_stream(ov2640_t *dev);
ov2640_status_t ov2640_set_frame_callback(ov2640_t *dev, ov2640_frame_cb_t cb, void *user_ctx);
ov2640_status_t ov2640_capture_jpeg(ov2640_t *dev, uint8_t *buf, uint32_t buf_cap, uint32_t *out_len);
bool ov2640_is_initialized(const ov2640_t *dev);
bool ov2640_pins_valid(const ov2640_pin_config_t *pins);

#ifdef __cplusplus
}
#endif

#endif /* OV2640_H */
