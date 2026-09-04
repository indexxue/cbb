/**
 * @file    epd_display.c
 * @brief   墨水屏绘图层（运行时宽高，板级帧缓冲）。
 */

#include "epd_display.h"

#include "epd_display_font.h"

#include <stdio.h>
#include <string.h>

static epd_display_status_t map_status(epd_status_t st)
{
    switch (st) {
    case EPD_OK:
        return EPD_DISPLAY_OK;
    case EPD_ERROR_PARAM:
        return EPD_DISPLAY_ERROR_PARAM;
    case EPD_ERROR_NOT_INIT:
        return EPD_DISPLAY_ERROR_NOT_INIT;
    case EPD_ERROR_BUSY:
        return EPD_DISPLAY_ERROR_BUSY;
    case EPD_ERROR_UNSUPPORTED:
        return EPD_DISPLAY_ERROR_UNSUPPORTED;
    default:
        return EPD_DISPLAY_ERROR_BUSY;
    }
}

static uint16_t disp_w(const epd_display_t *disp)
{
    return epd_width(&disp->epd);
}

static uint16_t disp_h(const epd_display_t *disp)
{
    return epd_height(&disp->epd);
}

static uint16_t disp_row_bytes(const epd_display_t *disp)
{
    return epd_bytes_per_row(&disp->epd);
}

static uint32_t disp_plane(const epd_display_t *disp)
{
    return epd_plane_size(&disp->epd);
}

static bool panel_flip_y(const epd_display_t *disp)
{
    return (disp->epd.panel != NULL) && disp->epd.panel->flip_y;
}

static void plane_fill(uint8_t *plane, uint32_t size, bool bit_val)
{
    if (plane == NULL || size == 0U) {
        return;
    }
    memset(plane, bit_val ? 0xFFU : 0x00U, size);
}

static uint16_t map_y_to_ram(const epd_display_t *disp, uint16_t y)
{
    const uint16_t h = disp_h(disp);

    if (!panel_flip_y(disp) || y >= h) {
        return y;
    }
    return (uint16_t)((h - 1U) - y);
}

static void set_bw_bit(epd_display_t *disp, uint16_t x, uint16_t y, bool white)
{
    const uint16_t ry = map_y_to_ram(disp, y);
    const uint16_t w = disp_w(disp);
    const uint16_t h = disp_h(disp);
    uint16_t bx;
    uint8_t bit;
    uint32_t idx;
    uint8_t mask;

    if (disp == NULL || disp->bw == NULL || x >= w || ry >= h) {
        return;
    }

    bx = (uint16_t)(x / 8U);
    bit = (uint8_t)(7U - (x % 8U));
    idx = (uint32_t)ry * (uint32_t)disp_row_bytes(disp) + bx;
    mask = (uint8_t)(1U << bit);

    if (white) {
        disp->bw[idx] |= mask;
    } else {
        disp->bw[idx] &= (uint8_t)~mask;
    }
}

static void set_red_bit(epd_display_t *disp, uint16_t x, uint16_t y, bool red)
{
    const uint16_t ry = map_y_to_ram(disp, y);
    const uint16_t w = disp_w(disp);
    const uint16_t h = disp_h(disp);
    uint16_t bx;
    uint8_t bit;
    uint32_t idx;
    uint8_t mask;

    if (disp == NULL || disp->red == NULL || x >= w || ry >= h) {
        return;
    }

    bx = (uint16_t)(x / 8U);
    bit = (uint8_t)(7U - (x % 8U));
    idx = (uint32_t)ry * (uint32_t)disp_row_bytes(disp) + bx;
    mask = (uint8_t)(1U << bit);

    if (red) {
        disp->red[idx] |= mask;
    } else {
        disp->red[idx] &= (uint8_t)~mask;
    }
}

static bool get_bw_bit(const epd_display_t *disp, uint16_t x, uint16_t y)
{
    const uint16_t ry = map_y_to_ram(disp, y);
    const uint16_t bx = (uint16_t)(x / 8U);
    const uint8_t bit = (uint8_t)(7U - (x % 8U));
    const uint32_t idx = (uint32_t)ry * (uint32_t)disp_row_bytes(disp) + bx;

    return (disp->bw[idx] & (uint8_t)(1U << bit)) != 0U;
}

static bool get_red_bit(const epd_display_t *disp, uint16_t x, uint16_t y)
{
    const uint16_t ry = map_y_to_ram(disp, y);
    const uint16_t bx = (uint16_t)(x / 8U);
    const uint8_t bit = (uint8_t)(7U - (x % 8U));
    const uint32_t idx = (uint32_t)ry * (uint32_t)disp_row_bytes(disp) + bx;

    if (disp->red == NULL) {
        return false;
    }
    return (disp->red[idx] & (uint8_t)(1U << bit)) != 0U;
}

static void apply_color_pixel(epd_display_t *disp, uint16_t x, uint16_t y, epd_color_t color)
{
    switch (color) {
    case EPD_COLOR_WHITE:
        set_bw_bit(disp, x, y, true);
        set_red_bit(disp, x, y, false);
        break;
    case EPD_COLOR_BLACK:
        set_bw_bit(disp, x, y, false);
        set_red_bit(disp, x, y, false);
        break;
    case EPD_COLOR_RED:
        set_bw_bit(disp, x, y, true);
        set_red_bit(disp, x, y, true);
        break;
    default:
        break;
    }
}

static void clamp_rect(const epd_display_t *disp, uint16_t *x0, uint16_t *y0, uint16_t *x1,
                       uint16_t *y1)
{
    const uint16_t w = disp_w(disp);
    const uint16_t h = disp_h(disp);

    if (*x1 >= w) {
        *x1 = (uint16_t)(w - 1U);
    }
    if (*y1 >= h) {
        *y1 = (uint16_t)(h - 1U);
    }
    (void)x0;
    (void)y0;
}

static const unsigned char *font_glyph(char ch, epd_font_size_t size, uint8_t *glyph_bytes)
{
    const uint8_t idx = (uint8_t)(ch - ' ');

    if (ch < ' ' || ch > '~') {
        return NULL;
    }

    switch (size) {
    case EPD_FONT_12:
        *glyph_bytes = 12U;
        return epd_font_asc12[idx];
    case EPD_FONT_16:
        *glyph_bytes = 16U;
        return epd_font_asc16[idx];
    case EPD_FONT_24:
        *glyph_bytes = 36U;
        return epd_font_asc24[idx];
    default:
        return NULL;
    }
}

static uint8_t font_width(epd_font_size_t size)
{
    return (uint8_t)((uint8_t)size / 2U);
}

static void fill_test_pattern(epd_display_t *disp)
{
    const uint16_t w = disp_w(disp);
    const uint16_t h = disp_h(disp);
    const uint16_t mid_x = (uint16_t)(w / 2U);
    uint16_t x;
    uint16_t y;

    epd_display_clear_buffer(disp, EPD_COLOR_WHITE);

    if (h < 50U || mid_x == 0U) {
        return;
    }

    epd_display_fill_rect(disp, 0U, 0U, (uint16_t)(mid_x - 1U), 49U, EPD_COLOR_RED);
    for (x = 0U; x < w; x = (uint16_t)(x + 16U)) {
        epd_display_fill_rect(disp, x, 50U, (uint16_t)(x + 7U), 99U, EPD_COLOR_RED);
    }

    if (h > 149U) {
        epd_display_fill_rect(disp, 0U, 100U, (uint16_t)(w - 1U), 149U, EPD_COLOR_BLACK);
        if (w > 16U) {
            epd_display_fill_rect(disp, 8U, 118U, (uint16_t)(w - 9U), 131U, EPD_COLOR_RED);
        }
    }

    if (h > 199U) {
        epd_display_fill_rect(disp, 0U, 150U, 31U, 199U, EPD_COLOR_RED);
        for (y = 150U; y <= 199U; y++) {
            for (x = 40U; x < w; x++) {
                const bool black = (((x / 16U) + (y / 16U)) % 2U) == 0U;
                apply_color_pixel(disp, x, y, black ? EPD_COLOR_BLACK : EPD_COLOR_WHITE);
            }
        }
    }

    if (h > 200U) {
        epd_display_draw_rect(disp, 0U, 200U, (uint16_t)(w - 1U), (uint16_t)(h - 1U), EPD_COLOR_RED);
    }
}

epd_display_status_t epd_display_init(epd_display_t *disp, const epd_display_config_t *cfg)
{
    epd_status_t st;
    uint32_t need;

    if (disp == NULL || cfg == NULL || cfg->epd.panel == NULL || cfg->bw == NULL) {
        return EPD_DISPLAY_ERROR_PARAM;
    }

    need = epd_panel_plane_size(cfg->epd.panel);
    if (cfg->plane_capacity < need) {
        return EPD_DISPLAY_ERROR_PARAM;
    }
    if (cfg->epd.panel->planes >= 2U && cfg->red == NULL) {
        return EPD_DISPLAY_ERROR_PARAM;
    }

    if (disp->ready) {
        return EPD_DISPLAY_OK;
    }

    disp->bw = cfg->bw;
    disp->red = cfg->red;
    disp->plane_capacity = cfg->plane_capacity;
    disp->power_on = cfg->power_on;
    disp->power_off = cfg->power_off;
    disp->delay_ms = cfg->epd.delay_ms;

    if (disp->power_on != NULL) {
        disp->power_on();
    }

    st = epd_register(&disp->epd, &cfg->epd);
    if (st != EPD_OK) {
        if (disp->power_off != NULL) {
            disp->power_off();
        }
        return map_status(st);
    }

    st = epd_init(&disp->epd);
    if (st != EPD_OK) {
        if (disp->power_off != NULL) {
            disp->power_off();
        }
        return map_status(st);
    }

    if (cfg->hw_clear_on_init) {
        st = epd_clear(&disp->epd);
        if (st != EPD_OK) {
            if (disp->power_off != NULL) {
                disp->power_off();
            }
            return map_status(st);
        }
    }

    epd_display_clear_buffer(disp, EPD_COLOR_WHITE);
    disp->ready = true;
    return EPD_DISPLAY_OK;
}

epd_display_status_t epd_display_deinit(epd_display_t *disp, bool sleep)
{
    if (disp == NULL || !disp->ready) {
        return EPD_DISPLAY_ERROR_NOT_INIT;
    }

    if (sleep) {
        (void)epd_sleep(&disp->epd);
    }

    if (disp->power_off != NULL) {
        disp->power_off();
    }

    disp->ready = false;
    return EPD_DISPLAY_OK;
}

bool epd_display_is_ready(const epd_display_t *disp)
{
    return (disp != NULL) && disp->ready;
}

uint16_t epd_display_width(const epd_display_t *disp)
{
    if (disp == NULL) {
        return 0U;
    }
    return disp_w(disp);
}

uint16_t epd_display_height(const epd_display_t *disp)
{
    if (disp == NULL) {
        return 0U;
    }
    return disp_h(disp);
}

uint16_t epd_display_bytes_per_row(const epd_display_t *disp)
{
    if (disp == NULL) {
        return 0U;
    }
    return disp_row_bytes(disp);
}

uint32_t epd_display_plane_size(const epd_display_t *disp)
{
    if (disp == NULL) {
        return 0U;
    }
    return disp_plane(disp);
}

void epd_display_clear_buffer(epd_display_t *disp, epd_color_t color)
{
    const uint32_t size = (disp != NULL) ? disp_plane(disp) : 0U;

    if (disp == NULL || size == 0U) {
        return;
    }

    switch (color) {
    case EPD_COLOR_WHITE:
        plane_fill(disp->bw, size, true);
        plane_fill(disp->red, size, false);
        break;
    case EPD_COLOR_BLACK:
        plane_fill(disp->bw, size, false);
        plane_fill(disp->red, size, false);
        break;
    case EPD_COLOR_RED:
        plane_fill(disp->bw, size, true);
        plane_fill(disp->red, size, true);
        break;
    default:
        break;
    }
}

epd_display_status_t epd_display_clear_screen(epd_display_t *disp)
{
    if (disp == NULL || !disp->ready) {
        return EPD_DISPLAY_ERROR_NOT_INIT;
    }

    epd_display_clear_buffer(disp, EPD_COLOR_WHITE);
    return map_status(epd_clear(&disp->epd));
}

epd_display_status_t epd_display_refresh_mono(epd_display_t *disp)
{
    if (disp == NULL || !disp->ready) {
        return EPD_DISPLAY_ERROR_NOT_INIT;
    }

    return map_status(epd_display(&disp->epd, disp->bw, NULL));
}

epd_display_status_t epd_display_refresh(epd_display_t *disp)
{
    if (disp == NULL || !disp->ready) {
        return EPD_DISPLAY_ERROR_NOT_INIT;
    }

    if (disp->epd.panel->planes >= 2U) {
        return map_status(epd_display(&disp->epd, disp->bw, disp->red));
    }
    return map_status(epd_display(&disp->epd, disp->bw, NULL));
}

void epd_display_set_pixel(epd_display_t *disp, uint16_t x, uint16_t y, epd_color_t color)
{
    if (disp == NULL) {
        return;
    }
    apply_color_pixel(disp, x, y, color);
}

epd_color_t epd_display_get_pixel(const epd_display_t *disp, uint16_t x, uint16_t y)
{
    if (disp == NULL || x >= disp_w(disp) || y >= disp_h(disp)) {
        return EPD_COLOR_WHITE;
    }

    if (get_red_bit(disp, x, y)) {
        return EPD_COLOR_RED;
    }
    return get_bw_bit(disp, x, y) ? EPD_COLOR_WHITE : EPD_COLOR_BLACK;
}

void epd_display_fill_rect(epd_display_t *disp, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                           epd_color_t color)
{
    uint16_t y;
    uint16_t x;

    if (disp == NULL) {
        return;
    }

    clamp_rect(disp, &x0, &y0, &x1, &y1);

    for (y = y0; y <= y1; y++) {
        for (x = x0; x <= x1; x++) {
            apply_color_pixel(disp, x, y, color);
        }
    }
}

void epd_display_draw_rect(epd_display_t *disp, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                           epd_color_t color)
{
    epd_display_draw_line(disp, x0, y0, x1, y0, color);
    epd_display_draw_line(disp, x0, y1, x1, y1, color);
    epd_display_draw_line(disp, x0, y0, x0, y1, color);
    epd_display_draw_line(disp, x1, y0, x1, y1, color);
}

void epd_display_draw_line(epd_display_t *disp, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                           epd_color_t color)
{
    int16_t dx;
    int16_t dy;
    int16_t sx;
    int16_t sy;
    int16_t err;
    int16_t x;
    int16_t y;

    if (disp == NULL) {
        return;
    }

    dx = (int16_t)((x1 >= x0) ? (x1 - x0) : (x0 - x1));
    dy = (int16_t)((y1 >= y0) ? (y1 - y0) : (y0 - y1));
    sx = (x0 < x1) ? 1 : -1;
    sy = (y0 < y1) ? 1 : -1;
    err = dx - dy;
    x = (int16_t)x0;
    y = (int16_t)y0;

    for (;;) {
        if (x >= 0 && y >= 0) {
            apply_color_pixel(disp, (uint16_t)x, (uint16_t)y, color);
        }

        if (x == (int16_t)x1 && y == (int16_t)y1) {
            break;
        }

        {
            const int16_t e2 = (int16_t)(2 * err);
            if (e2 > -dy) {
                err -= dy;
                x += sx;
            }
            if (e2 < dx) {
                err += dx;
                y += sy;
            }
        }
    }
}

void epd_display_draw_bitmap(epd_display_t *disp, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                             const uint8_t *bitmap, epd_color_t fg, epd_color_t bg)
{
    uint16_t row_bytes;
    uint16_t row;
    uint16_t col;

    if (disp == NULL || bitmap == NULL || w == 0U || h == 0U) {
        return;
    }

    row_bytes = (uint16_t)((w + 7U) / 8U);

    for (row = 0U; row < h; row++) {
        for (col = 0U; col < w; col++) {
            const uint32_t byte_idx = (uint32_t)row * row_bytes + (col / 8U);
            const uint8_t mask = (uint8_t)(0x80U >> (col % 8U));
            const epd_color_t c = (bitmap[byte_idx] & mask) != 0U ? fg : bg;

            apply_color_pixel(disp, (uint16_t)(x + col), (uint16_t)(y + row), c);
        }
    }
}

void epd_display_draw_icon(epd_display_t *disp, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                           const uint8_t *icon, epd_color_t fg, epd_color_t bg)
{
    epd_display_draw_bitmap(disp, x, y, w, h, icon, fg, bg);
}

void epd_display_draw_gfx_asset(epd_display_t *disp, uint16_t x, uint16_t y, epd_gfx_asset_id_t asset,
                                epd_color_t fg, epd_color_t bg)
{
    const epd_gfx_asset_t *res = epd_gfx_asset_get(asset);

    if (res == NULL || res->data == NULL) {
        return;
    }

    epd_display_draw_bitmap(disp, x, y, res->width, res->height, res->data, fg, bg);
}

void epd_display_draw_shapes_sample(epd_display_t *disp, uint16_t x0, uint16_t y0, epd_color_t color)
{
    if (disp == NULL) {
        return;
    }

    epd_display_fill_rect(disp, x0, (uint16_t)(y0 + 14U), (uint16_t)(x0 + 31U), (uint16_t)(y0 + 45U),
                          color);
    epd_display_draw_rect(disp, (uint16_t)(x0 + 40U), (uint16_t)(y0 + 14U), (uint16_t)(x0 + 75U),
                          (uint16_t)(y0 + 45U), color);
    epd_display_draw_line(disp, x0, (uint16_t)(y0 + 52U), (uint16_t)(x0 + 75U), (uint16_t)(y0 + 52U),
                          color);
    epd_display_draw_line(disp, (uint16_t)(x0 + 40U), (uint16_t)(y0 + 52U), (uint16_t)(x0 + 75U),
                          (uint16_t)(y0 + 79U), color);
    epd_display_draw_line(disp, x0, (uint16_t)(y0 + 79U), (uint16_t)(x0 + 75U), (uint16_t)(y0 + 52U),
                          color);
}

void epd_display_gfx_demo_fill(epd_display_t *disp)
{
    const epd_font_size_t font = EPD_FONT_12;
    const uint16_t w = (disp != NULL) ? disp_w(disp) : 0U;

    if (disp == NULL || w == 0U) {
        return;
    }

    epd_display_fill_rect(disp, 0U, 0U, (uint16_t)(w - 1U), 3U, EPD_COLOR_BLACK);
    epd_display_draw_string(disp, 4U, 8U, "GFX/ICON TEST", font, EPD_COLOR_BLACK);

    epd_display_draw_string(disp, 4U, 24U, "SHAPES:", font, EPD_COLOR_BLACK);
    epd_display_draw_shapes_sample(disp, 4U, 24U, EPD_COLOR_BLACK);

    epd_display_draw_string(disp, 4U, 110U, "ICONS:", font, EPD_COLOR_BLACK);
    epd_display_draw_gfx_asset(disp, 4U, 124U, EPD_GFX_ICON_HEART_16, EPD_COLOR_BLACK, EPD_COLOR_WHITE);
    epd_display_draw_gfx_asset(disp, 28U, 124U, EPD_GFX_ICON_CHECK_16, EPD_COLOR_BLACK,
                               EPD_COLOR_WHITE);
    epd_display_draw_gfx_asset(disp, 52U, 124U, EPD_GFX_ICON_ARROW_16, EPD_COLOR_BLACK,
                               EPD_COLOR_WHITE);

    epd_display_draw_string(disp, 4U, 146U, "BITMAP:", font, EPD_COLOR_BLACK);
    epd_display_draw_gfx_asset(disp, 4U, 160U, EPD_GFX_BITMAP_CHECKER_32, EPD_COLOR_BLACK,
                               EPD_COLOR_WHITE);

    epd_display_draw_string(disp, 4U, 200U, "RED ICON:", font, EPD_COLOR_BLACK);
    epd_display_draw_gfx_asset(disp, 4U, 214U, EPD_GFX_ICON_HEART_16, EPD_COLOR_RED, EPD_COLOR_WHITE);

    epd_display_draw_string(disp, 4U, 238U, "GFX TEST OK", font, EPD_COLOR_BLACK);
}

epd_display_status_t epd_display_verify_gfx(epd_display_t *disp)
{
    if (disp == NULL || !disp->ready) {
        return EPD_DISPLAY_ERROR_NOT_INIT;
    }

    epd_display_clear_buffer(disp, EPD_COLOR_WHITE);
    epd_display_gfx_demo_fill(disp);
    return epd_display_refresh(disp);
}

void epd_display_draw_char(epd_display_t *disp, uint16_t x, uint16_t y, char ch, epd_font_size_t size,
                           epd_color_t color)
{
    uint8_t glyph_bytes = 0U;
    const unsigned char *glyph = font_glyph(ch, size, &glyph_bytes);
    uint16_t px = x;
    uint16_t py = y;
    const uint16_t y0 = y;
    uint8_t i;
    uint8_t m;
    uint8_t temp;

    if (disp == NULL || glyph == NULL) {
        return;
    }

    for (i = 0U; i < glyph_bytes; i++) {
        temp = glyph[i];

        for (m = 0U; m < 8U; m++) {
            /* 仅画前景像素，背景透明，避免白底方块盖住下层黑/红 */
            if ((temp & 0x80U) != 0U) {
                apply_color_pixel(disp, px, py, color);
            }

            temp <<= 1U;
            py++;
            if ((py - y0) == (uint16_t)size) {
                py = y0;
                px++;
                break;
            }
        }
    }
}

void epd_display_draw_string(epd_display_t *disp, uint16_t x, uint16_t y, const char *str,
                             epd_font_size_t size, epd_color_t color)
{
    uint16_t cx = x;
    uint16_t cy = y;
    const uint8_t step = font_width(size);
    const uint16_t w = (disp != NULL) ? disp_w(disp) : 0U;
    const uint16_t h = (disp != NULL) ? disp_h(disp) : 0U;

    if (disp == NULL || str == NULL || w == 0U) {
        return;
    }

    while (*str != '\0') {
        if (*str < ' ' || *str > '~') {
            str++;
            continue;
        }

        epd_display_draw_char(disp, cx, cy, *str, size, color);
        cx = (uint16_t)(cx + step);

        if (cx > (uint16_t)(w - step)) {
            cx = 0U;
            cy = (uint16_t)(cy + size);
            if (cy >= h) {
                break;
            }
        }
        str++;
    }
}

void epd_display_draw_num(epd_display_t *disp, uint16_t x, uint16_t y, uint32_t num, uint8_t len,
                          epd_font_size_t size, epd_color_t color)
{
    char buf[12];
    uint8_t i;

    if (disp == NULL || len == 0U || len >= sizeof(buf)) {
        return;
    }

    (void)snprintf(buf, sizeof(buf), "%0*lu", (int)len, (unsigned long)num);
    for (i = 0U; i < len; i++) {
        epd_display_draw_char(disp, (uint16_t)(x + font_width(size) * i), y, buf[i], size, color);
    }
}

const uint8_t *epd_display_bw_plane(const epd_display_t *disp)
{
    if (disp == NULL) {
        return NULL;
    }
    return disp->bw;
}

const uint8_t *epd_display_red_plane(const epd_display_t *disp)
{
    if (disp == NULL) {
        return NULL;
    }
    return disp->red;
}

epd_display_status_t epd_display_test_solid(epd_display_t *disp, epd_color_t color)
{
    if (disp == NULL || !disp->ready) {
        return EPD_DISPLAY_ERROR_NOT_INIT;
    }

    epd_display_clear_buffer(disp, color);

    if (color == EPD_COLOR_RED) {
        return epd_display_refresh(disp);
    }

    return epd_display_refresh_mono(disp);
}

void epd_display_test_pattern_fill(epd_display_t *disp)
{
    if (disp == NULL) {
        return;
    }
    fill_test_pattern(disp);
}

epd_display_status_t epd_display_test_run(epd_display_t *disp)
{
    epd_display_status_t st;

    if (disp == NULL) {
        return EPD_DISPLAY_ERROR_PARAM;
    }

    st = epd_display_test_solid(disp, EPD_COLOR_BLACK);
    if (st != EPD_DISPLAY_OK) {
        return st;
    }
    if (disp->delay_ms != NULL) {
        disp->delay_ms(2000U);
    }

    st = epd_display_test_solid(disp, EPD_COLOR_WHITE);
    if (st != EPD_DISPLAY_OK) {
        return st;
    }
    if (disp->delay_ms != NULL) {
        disp->delay_ms(2000U);
    }

    st = epd_display_test_solid(disp, EPD_COLOR_RED);
    if (st != EPD_DISPLAY_OK) {
        return st;
    }
    if (disp->delay_ms != NULL) {
        disp->delay_ms(2000U);
    }

    epd_display_test_pattern_fill(disp);
    return epd_display_refresh(disp);
}
