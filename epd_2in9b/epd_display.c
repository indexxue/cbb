/**
 * @file    epd_display.c
 * @brief   2.9 寸红白黑墨水屏绘图层（GDEY029Z95 / SSD1680）。
 */

#include "epd_display.h"

#include "epd_display_font.h"

#include <stdio.h>
#include <string.h>

static epd_display_status_t map_status(epd_2in9b_status_t st)
{
    switch (st) {
    case EPD_2IN9B_OK:
        return EPD_DISPLAY_OK;
    case EPD_2IN9B_ERROR_PARAM:
        return EPD_DISPLAY_ERROR_PARAM;
    case EPD_2IN9B_ERROR_NOT_INIT:
        return EPD_DISPLAY_ERROR_NOT_INIT;
    case EPD_2IN9B_ERROR_BUSY:
        return EPD_DISPLAY_ERROR_BUSY;
    default:
        return EPD_DISPLAY_ERROR_BUSY;
    }
}

static void plane_fill(uint8_t *plane, bool bit_val)
{
    memset(plane, bit_val ? 0xFFU : 0x00U, EPD_2IN9B_PLANE_SIZE);
}

static uint16_t map_y_to_ram(uint16_t y)
{
    if (y >= EPD_DISPLAY_HEIGHT) {
        return y;
    }
    return (uint16_t)((EPD_DISPLAY_HEIGHT - 1U) - y);
}

static void set_bw_bit(epd_display_t *disp, uint16_t x, uint16_t y, bool white)
{
    const uint16_t ry = map_y_to_ram(y);

    if (disp == NULL || x >= EPD_DISPLAY_WIDTH || ry >= EPD_DISPLAY_HEIGHT) {
        return;
    }

    const uint16_t bx = (uint16_t)(x / 8U);
    const uint8_t bit = (uint8_t)(7U - (x % 8U));
    const uint32_t idx = (uint32_t)ry * EPD_2IN9B_BYTES_PER_ROW + bx;
    const uint8_t mask = (uint8_t)(1U << bit);

    if (white) {
        disp->bw[idx] |= mask;
    } else {
        disp->bw[idx] &= (uint8_t)~mask;
    }
}

static void set_red_bit(epd_display_t *disp, uint16_t x, uint16_t y, bool red)
{
    const uint16_t ry = map_y_to_ram(y);

    if (disp == NULL || x >= EPD_DISPLAY_WIDTH || ry >= EPD_DISPLAY_HEIGHT) {
        return;
    }

    const uint16_t bx = (uint16_t)(x / 8U);
    const uint8_t bit = (uint8_t)(7U - (x % 8U));
    const uint32_t idx = (uint32_t)ry * EPD_2IN9B_BYTES_PER_ROW + bx;
    const uint8_t mask = (uint8_t)(1U << bit);

    if (red) {
        disp->red[idx] |= mask;
    } else {
        disp->red[idx] &= (uint8_t)~mask;
    }
}

static bool get_bw_bit(const epd_display_t *disp, uint16_t x, uint16_t y)
{
    const uint16_t ry = map_y_to_ram(y);
    const uint16_t bx = (uint16_t)(x / 8U);
    const uint8_t bit = (uint8_t)(7U - (x % 8U));
    const uint32_t idx = (uint32_t)ry * EPD_2IN9B_BYTES_PER_ROW + bx;

    return (disp->bw[idx] & (uint8_t)(1U << bit)) != 0U;
}

static bool get_red_bit(const epd_display_t *disp, uint16_t x, uint16_t y)
{
    const uint16_t ry = map_y_to_ram(y);
    const uint16_t bx = (uint16_t)(x / 8U);
    const uint8_t bit = (uint8_t)(7U - (x % 8U));
    const uint32_t idx = (uint32_t)ry * EPD_2IN9B_BYTES_PER_ROW + bx;

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

static void clamp_rect(uint16_t *x0, uint16_t *y0, uint16_t *x1, uint16_t *y1)
{
    if (*x1 >= EPD_DISPLAY_WIDTH) {
        *x1 = (uint16_t)(EPD_DISPLAY_WIDTH - 1U);
    }
    if (*y1 >= EPD_DISPLAY_HEIGHT) {
        *y1 = (uint16_t)(EPD_DISPLAY_HEIGHT - 1U);
    }
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
    const uint16_t mid_x = (uint16_t)(EPD_DISPLAY_WIDTH / 2U);

    epd_display_clear_buffer(disp, EPD_COLOR_WHITE);

    epd_display_fill_rect(disp, 0U, 0U, (uint16_t)(mid_x - 1U), 49U, EPD_COLOR_RED);
    for (uint16_t x = 0U; x < EPD_DISPLAY_WIDTH; x = (uint16_t)(x + 16U)) {
        epd_display_fill_rect(disp, x, 50U, (uint16_t)(x + 7U), 99U, EPD_COLOR_RED);
    }

    epd_display_fill_rect(disp, 0U, 100U, (uint16_t)(EPD_DISPLAY_WIDTH - 1U), 149U, EPD_COLOR_BLACK);
    epd_display_fill_rect(disp, 8U, 118U, (uint16_t)(EPD_DISPLAY_WIDTH - 9U), 131U, EPD_COLOR_RED);

    epd_display_fill_rect(disp, 0U, 150U, 31U, 199U, EPD_COLOR_RED);
    for (uint16_t y = 150U; y <= 199U; y++) {
        for (uint16_t x = 40U; x < EPD_DISPLAY_WIDTH; x++) {
            const bool black = (((x / 16U) + (y / 16U)) % 2U) == 0U;
            apply_color_pixel(disp, x, y, black ? EPD_COLOR_BLACK : EPD_COLOR_WHITE);
        }
    }

    epd_display_draw_rect(disp, 0U, 200U, (uint16_t)(EPD_DISPLAY_WIDTH - 1U),
                          (uint16_t)(EPD_DISPLAY_HEIGHT - 1U), EPD_COLOR_RED);
}

epd_display_status_t epd_display_init(epd_display_t *disp, const epd_display_config_t *cfg)
{
    epd_2in9b_status_t st;

    if (disp == NULL || cfg == NULL) {
        return EPD_DISPLAY_ERROR_PARAM;
    }

    if (disp->ready) {
        return EPD_DISPLAY_OK;
    }

    disp->power_on = cfg->power_on;
    disp->power_off = cfg->power_off;
    disp->delay_ms = cfg->epd.delay_ms;

    if (disp->power_on != NULL) {
        disp->power_on();
    }

    st = epd_2in9b_register(&disp->epd, &cfg->epd);
    if (st != EPD_2IN9B_OK) {
        if (disp->power_off != NULL) {
            disp->power_off();
        }
        return map_status(st);
    }

    st = epd_2in9b_init(&disp->epd);
    if (st != EPD_2IN9B_OK) {
        if (disp->power_off != NULL) {
            disp->power_off();
        }
        return map_status(st);
    }

    st = epd_2in9b_clear(&disp->epd);
    if (st != EPD_2IN9B_OK) {
        if (disp->power_off != NULL) {
            disp->power_off();
        }
        return map_status(st);
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
        (void)epd_2in9b_sleep(&disp->epd);
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

void epd_display_clear_buffer(epd_display_t *disp, epd_color_t color)
{
    if (disp == NULL) {
        return;
    }

    switch (color) {
    case EPD_COLOR_WHITE:
        plane_fill(disp->bw, true);
        plane_fill(disp->red, false);
        break;
    case EPD_COLOR_BLACK:
        plane_fill(disp->bw, false);
        plane_fill(disp->red, false);
        break;
    case EPD_COLOR_RED:
        plane_fill(disp->bw, true);
        plane_fill(disp->red, true);
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
    return map_status(epd_2in9b_clear(&disp->epd));
}

epd_display_status_t epd_display_refresh_mono(epd_display_t *disp)
{
    if (disp == NULL || !disp->ready) {
        return EPD_DISPLAY_ERROR_NOT_INIT;
    }

    plane_fill(disp->red, false);
    return map_status(epd_2in9b_display(&disp->epd, disp->bw, disp->red));
}

epd_display_status_t epd_display_refresh(epd_display_t *disp)
{
    if (disp == NULL || !disp->ready) {
        return EPD_DISPLAY_ERROR_NOT_INIT;
    }

    return map_status(epd_2in9b_display(&disp->epd, disp->bw, disp->red));
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
    if (disp == NULL || x >= EPD_DISPLAY_WIDTH || y >= EPD_DISPLAY_HEIGHT) {
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
    if (disp == NULL) {
        return;
    }

    clamp_rect(&x0, &y0, &x1, &y1);

    for (uint16_t y = y0; y <= y1; y++) {
        for (uint16_t x = x0; x <= x1; x++) {
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

    epd_display_fill_rect(disp, x0, (uint16_t)(y0 + 14U), (uint16_t)(x0 + 31U), (uint16_t)(y0 + 45U), color);
    epd_display_draw_rect(disp, (uint16_t)(x0 + 40U), (uint16_t)(y0 + 14U), (uint16_t)(x0 + 75U),
                          (uint16_t)(y0 + 45U), color);
    epd_display_draw_line(disp, x0, (uint16_t)(y0 + 52U), (uint16_t)(x0 + 75U), (uint16_t)(y0 + 52U), color);
    epd_display_draw_line(disp, (uint16_t)(x0 + 40U), (uint16_t)(y0 + 52U), (uint16_t)(x0 + 75U),
                          (uint16_t)(y0 + 79U), color);
    epd_display_draw_line(disp, x0, (uint16_t)(y0 + 79U), (uint16_t)(x0 + 75U), (uint16_t)(y0 + 52U), color);
}

void epd_display_gfx_demo_fill(epd_display_t *disp)
{
    const epd_font_size_t font = EPD_FONT_12;

    if (disp == NULL) {
        return;
    }

    epd_display_fill_rect(disp, 0U, 0U, 127U, 3U, EPD_COLOR_BLACK);
    epd_display_draw_string(disp, 4U, 8U, "GFX/ICON TEST", font, EPD_COLOR_BLACK);

    epd_display_draw_string(disp, 4U, 24U, "SHAPES:", font, EPD_COLOR_BLACK);
    epd_display_draw_shapes_sample(disp, 4U, 24U, EPD_COLOR_BLACK);

    epd_display_draw_string(disp, 4U, 110U, "ICONS:", font, EPD_COLOR_BLACK);
    epd_display_draw_gfx_asset(disp, 4U, 124U, EPD_GFX_ICON_HEART_16, EPD_COLOR_BLACK, EPD_COLOR_WHITE);
    epd_display_draw_gfx_asset(disp, 28U, 124U, EPD_GFX_ICON_CHECK_16, EPD_COLOR_BLACK, EPD_COLOR_WHITE);
    epd_display_draw_gfx_asset(disp, 52U, 124U, EPD_GFX_ICON_ARROW_16, EPD_COLOR_BLACK, EPD_COLOR_WHITE);

    epd_display_draw_string(disp, 4U, 146U, "BITMAP:", font, EPD_COLOR_BLACK);
    epd_display_draw_gfx_asset(disp, 4U, 160U, EPD_GFX_BITMAP_CHECKER_32, EPD_COLOR_BLACK, EPD_COLOR_WHITE);

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
            apply_color_pixel(disp, px, py, (temp & 0x80U) != 0U ? color : EPD_COLOR_WHITE);

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

    if (disp == NULL || str == NULL) {
        return;
    }

    while (*str != '\0') {
        if (*str < ' ' || *str > '~') {
            str++;
            continue;
        }

        epd_display_draw_char(disp, cx, cy, *str, size, color);
        cx = (uint16_t)(cx + step);

        if (cx > (uint16_t)(EPD_DISPLAY_WIDTH - step)) {
            cx = 0U;
            cy = (uint16_t)(cy + size);
            if (cy >= EPD_DISPLAY_HEIGHT) {
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

uint16_t epd_display_plane_size(void)
{
    return (uint16_t)EPD_2IN9B_PLANE_SIZE;
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
