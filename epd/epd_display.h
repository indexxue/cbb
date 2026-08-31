/**
 * @file    epd_display.h
 * @brief   墨水屏绘图层（panel profile + 板级帧缓冲，MCU 无关）。
 *
 * 逻辑坐标：原点左上角；宽高取自当前 panel。
 */

#ifndef EPD_DISPLAY_H
#define EPD_DISPLAY_H

#include "epd.h"
#include "epd_panel.h"
#include "epd_display_assets.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EPD_DISPLAY_OK = 0,
    EPD_DISPLAY_ERROR_PARAM,
    EPD_DISPLAY_ERROR_NOT_INIT,
    EPD_DISPLAY_ERROR_BUSY,
    EPD_DISPLAY_ERROR_UNSUPPORTED,
} epd_display_status_t;

typedef enum {
    EPD_COLOR_WHITE = 0,
    EPD_COLOR_BLACK,
    EPD_COLOR_RED,
} epd_color_t;

typedef enum {
    EPD_FONT_12 = 12,
    EPD_FONT_16 = 16,
    EPD_FONT_24 = 24,
} epd_font_size_t;

typedef struct {
    epd_config_t epd;
    uint8_t *bw;
    uint8_t *red;              /**< BW panel 可 NULL。 */
    uint32_t plane_capacity;   /**< 必须 >= epd_panel_plane_size(panel)。 */
    void (*power_on)(void);
    void (*power_off)(void);
    /** true：init 后硬件全刷清屏；默认 false，仅清 MCU 帧缓冲。 */
    bool hw_clear_on_init;
} epd_display_config_t;

typedef struct {
    epd_t epd;
    uint8_t *bw;
    uint8_t *red;
    uint32_t plane_capacity;
    bool ready;
    void (*power_on)(void);
    void (*power_off)(void);
    epd_delay_ms_t delay_ms;
} epd_display_t;

epd_display_status_t epd_display_init(epd_display_t *disp, const epd_display_config_t *cfg);
epd_display_status_t epd_display_deinit(epd_display_t *disp, bool sleep);
bool epd_display_is_ready(const epd_display_t *disp);

uint16_t epd_display_width(const epd_display_t *disp);
uint16_t epd_display_height(const epd_display_t *disp);
uint16_t epd_display_bytes_per_row(const epd_display_t *disp);
uint32_t epd_display_plane_size(const epd_display_t *disp);

void epd_display_clear_buffer(epd_display_t *disp, epd_color_t color);
epd_display_status_t epd_display_clear_screen(epd_display_t *disp);
epd_display_status_t epd_display_refresh_mono(epd_display_t *disp);
epd_display_status_t epd_display_refresh(epd_display_t *disp);

void epd_display_set_pixel(epd_display_t *disp, uint16_t x, uint16_t y, epd_color_t color);
epd_color_t epd_display_get_pixel(const epd_display_t *disp, uint16_t x, uint16_t y);

void epd_display_fill_rect(epd_display_t *disp, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                           epd_color_t color);
void epd_display_draw_rect(epd_display_t *disp, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                           epd_color_t color);
void epd_display_draw_line(epd_display_t *disp, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                           epd_color_t color);

void epd_display_draw_bitmap(epd_display_t *disp, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                             const uint8_t *bitmap, epd_color_t fg, epd_color_t bg);
void epd_display_draw_icon(epd_display_t *disp, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                           const uint8_t *icon, epd_color_t fg, epd_color_t bg);

void epd_display_draw_gfx_asset(epd_display_t *disp, uint16_t x, uint16_t y, epd_gfx_asset_id_t asset,
                                epd_color_t fg, epd_color_t bg);
void epd_display_draw_shapes_sample(epd_display_t *disp, uint16_t x0, uint16_t y0, epd_color_t color);
void epd_display_gfx_demo_fill(epd_display_t *disp);
epd_display_status_t epd_display_verify_gfx(epd_display_t *disp);

void epd_display_draw_char(epd_display_t *disp, uint16_t x, uint16_t y, char ch, epd_font_size_t size,
                           epd_color_t color);
void epd_display_draw_string(epd_display_t *disp, uint16_t x, uint16_t y, const char *str,
                             epd_font_size_t size, epd_color_t color);
void epd_display_draw_num(epd_display_t *disp, uint16_t x, uint16_t y, uint32_t num, uint8_t len,
                          epd_font_size_t size, epd_color_t color);

const uint8_t *epd_display_bw_plane(const epd_display_t *disp);
const uint8_t *epd_display_red_plane(const epd_display_t *disp);

epd_display_status_t epd_display_test_solid(epd_display_t *disp, epd_color_t color);
void epd_display_test_pattern_fill(epd_display_t *disp);
epd_display_status_t epd_display_test_run(epd_display_t *disp);

#ifdef __cplusplus
}
#endif

#endif /* EPD_DISPLAY_H */
