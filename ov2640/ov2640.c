#include "ov2640.h"

#include <string.h>

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
    if ((cfg == NULL) || (cfg->sccb_write == NULL) || (cfg->sccb_read == NULL) || (cfg->frame_width == 0U) ||
        (cfg->frame_height == 0U)) {
        return false;
    }
    return ov2640_pins_valid(&cfg->pins);
}

ov2640_status_t ov2640_init_with_config(ov2640_t *dev, const ov2640_config_t *cfg)
{
    if ((dev == NULL) || !ov2640_cfg_valid(cfg)) {
        return OV2640_ERROR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->sccb_write      = cfg->sccb_write;
    dev->sccb_read       = cfg->sccb_read;
    dev->sccb_write_read = cfg->sccb_write_read;
    dev->delay_ms        = cfg->delay_ms;
    dev->set_pwdn        = cfg->set_pwdn;
    dev->set_reset       = cfg->set_reset;
    dev->sccb_addr7      = (cfg->sccb_addr7 != 0U) ? cfg->sccb_addr7 : OV2640_SCCB_ADDR_7BIT_DEFAULT;
    dev->pins            = cfg->pins;
    dev->frame_width     = cfg->frame_width;
    dev->frame_height    = cfg->frame_height;
    dev->format          = cfg->format;
    dev->initialized     = true;
    dev->streaming       = false;

    /* TODO(M3): SCCB 芯片 ID 检测、DVP/LEDC XCLK、esp_driver_cam 或等价采集后端。 */
    return OV2640_OK;
}

ov2640_status_t ov2640_start_stream(ov2640_t *dev)
{
    if ((dev == NULL) || !dev->initialized) {
        return OV2640_ERROR_NOT_INIT;
    }
    /* TODO(M3): 低帧率预览启动。 */
    dev->streaming = true;
    return OV2640_OK;
}

ov2640_status_t ov2640_stop_stream(ov2640_t *dev)
{
    if ((dev == NULL) || !dev->initialized) {
        return OV2640_ERROR_NOT_INIT;
    }
    dev->streaming = false;
    return OV2640_OK;
}

ov2640_status_t ov2640_capture_jpeg(ov2640_t *dev, uint8_t *buf, uint32_t buf_cap, uint32_t *out_len)
{
    if ((dev == NULL) || !dev->initialized || (buf == NULL) || (out_len == NULL) || (buf_cap == 0U)) {
        return OV2640_ERROR_PARAM;
    }
    /* TODO(M4): 单帧 JPEG 抓拍至 buf，写入 SD 由 voice_hub_storage 负责。 */
    *out_len = 0U;
    return OV2640_ERROR_CAPTURE;
}

bool ov2640_is_initialized(const ov2640_t *dev)
{
    return (dev != NULL) && dev->initialized;
}
