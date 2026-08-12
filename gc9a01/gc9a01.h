/**
 * @file    gc9a01.h
 * @brief   GC9A01 1.28" 240×240 圆屏：回调 SPI/引脚，与 st7789 同风格。
 */

#ifndef GC9A01_H
#define GC9A01_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GC9A01_PANEL_W 240U
#define GC9A01_PANEL_H 240U

typedef enum {
    GC9A01_ROT_0 = 0,
    GC9A01_ROT_90 = 1,
    GC9A01_ROT_180 = 2,
    GC9A01_ROT_270 = 3,
} gc9a01_rotation_t;

typedef void (*gc9a01_spi_tx_t)(const uint8_t *data, uint16_t len);
typedef void (*gc9a01_pin_t)(int high);
typedef void (*gc9a01_delay_ms_t)(uint32_t ms);

typedef enum {
    GC9A01_OK = 0,
    GC9A01_ERROR_PARAM,
    GC9A01_ERROR_NOT_INIT,
} gc9a01_status_t;

typedef struct {
    gc9a01_spi_tx_t spi_tx;
    gc9a01_pin_t set_cs;
    gc9a01_pin_t set_dc;
    gc9a01_pin_t set_rst; /* 可为 NULL；RST 脚为 -1 时板级回调空操作 */
    gc9a01_pin_t set_bl;
    gc9a01_delay_ms_t delay_ms;
    uint8_t rotation; /* 0..3，超出按 GC9A01_ROT_0 */
} gc9a01_config_t;

typedef struct {
    gc9a01_spi_tx_t spi_tx;
    gc9a01_pin_t set_cs;
    gc9a01_pin_t set_dc;
    gc9a01_pin_t set_rst;
    gc9a01_pin_t set_bl;
    gc9a01_delay_ms_t delay_ms;
    uint8_t rotation;
    bool initialized;
} gc9a01_t;

gc9a01_status_t gc9a01_register(gc9a01_t *dev, const gc9a01_config_t *cfg);

gc9a01_status_t gc9a01_set_backlight(gc9a01_t *dev, bool on);
gc9a01_status_t gc9a01_set_window(gc9a01_t *dev, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
gc9a01_status_t gc9a01_write_pixels(gc9a01_t *dev, const uint16_t *buf, uint32_t len);
gc9a01_status_t gc9a01_write_pixel_bytes(gc9a01_t *dev, const uint8_t *buf, uint32_t nbytes);
void gc9a01_end_write(gc9a01_t *dev);

bool gc9a01_is_initialized(const gc9a01_t *dev);
uint16_t gc9a01_display_width(const gc9a01_t *dev);
uint16_t gc9a01_display_height(const gc9a01_t *dev);
gc9a01_rotation_t gc9a01_get_rotation(const gc9a01_t *dev);

/** 纯色填充（smoke / 简单 UI）；坐标含起点不含终点风格与 lcd_fill 一致。 */
void gc9a01_fill(gc9a01_t *dev, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);

#ifdef __cplusplus
}
#endif

#endif /* GC9A01_H */
