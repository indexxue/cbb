/**
 * @file    st7796.h
 * @brief   ST7796 TFT：回调 SPI/引脚，MCU 无关；默认 320x480 RGB565。
 */

#ifndef ST7796_H
#define ST7796_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 物理面板 320x480；逻辑宽高由 `st7796_display_width/height` 随 rotation 变化。 */
#define ST7796_PANEL_W 320U
#define ST7796_PANEL_H 480U

/**
 * 显示方向。
 * 0/1：竖屏 320x480；2/3：横屏 480x320。
 */
typedef enum {
    ST7796_ROT_PORTRAIT_0 = 0,
    ST7796_ROT_PORTRAIT_180 = 1,
    ST7796_ROT_LANDSCAPE = 2,
    ST7796_ROT_LANDSCAPE_270 = 3,
} st7796_rotation_t;

typedef void (*st7796_spi_tx_t)(const uint8_t *data, uint16_t len);
typedef void (*st7796_pin_t)(int high);
typedef void (*st7796_delay_ms_t)(uint32_t ms);

typedef enum {
    ST7796_OK = 0,
    ST7796_ERROR_PARAM,
    ST7796_ERROR_NOT_INIT,
} st7796_status_t;

typedef struct {
    st7796_spi_tx_t spi_tx;
    st7796_pin_t set_cs;
    st7796_pin_t set_dc;
    st7796_pin_t set_rst; /* NULL = 无复位脚 */
    st7796_pin_t set_bl;  /* NULL = 无背光脚 */
    st7796_delay_ms_t delay_ms;
    /** 未设置或 >3 时按 `ST7796_ROT_PORTRAIT_0` 处理。 */
    uint8_t rotation;
    /**
     * 色序：非 0 = BGR（MADCTL bit3）；0 = RGB。
     * 多数 ST7796 模组为 BGR；红蓝反了就改 0。
     */
    uint8_t bgr;
} st7796_config_t;

typedef struct {
    st7796_spi_tx_t spi_tx;
    st7796_pin_t set_cs;
    st7796_pin_t set_dc;
    st7796_pin_t set_rst;
    st7796_pin_t set_bl;
    st7796_delay_ms_t delay_ms;
    uint8_t rotation;
    bool initialized;
} st7796_t;

st7796_status_t st7796_register(st7796_t *dev, const st7796_config_t *cfg);

st7796_status_t st7796_set_backlight(st7796_t *dev, bool on);
st7796_status_t st7796_set_window(st7796_t *dev, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
st7796_status_t st7796_write_pixels(st7796_t *dev, const uint16_t *buf, uint32_t len);
/**
 * 在已调用 `st7796_set_window` 且 CS 保持有效（RAMWR 后）时，按 RGB565 大端顺序发送原始字节流。
 * 单帧内多次调用时总长勿超过窗口像素数 * 2。
 */
st7796_status_t st7796_write_pixel_bytes(st7796_t *dev, const uint8_t *buf, uint32_t nbytes);
void st7796_end_write(st7796_t *dev);

bool st7796_is_initialized(const st7796_t *dev);

uint16_t st7796_display_width(const st7796_t *dev);
uint16_t st7796_display_height(const st7796_t *dev);
st7796_rotation_t st7796_get_rotation(const st7796_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* ST7796_H */
