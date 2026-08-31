/**
 * @file    epd_panel_ws_1in54_v2.c
 * @brief   Waveshare 1.54" e-Paper Module V2（200×200 BW / SSD1681）。
 */

#include "epd_panel.h"

#include <stddef.h>

#define LUT_SIZE      159U
#define UPDATE_FULL   0xC7U

static const uint8_t s_lut_full[LUT_SIZE] = {
    0x80, 0x48, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x48, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x48, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x48, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x01, 0x00, 0x08, 0x01, 0x00, 0x02,
    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00,
    0x22, 0x17, 0x41, 0x00, 0x32, 0x20,
};

static void set_windows(epd_t *dev)
{
    const uint16_t w = dev->panel->width;
    const uint16_t h = dev->panel->height;
    const uint16_t y0 = (uint16_t)(h - 1U);

    /* data entry 0x01：Y 递减，窗口 Ystart=H-1, Yend=0 */
    epd_bus_write_cmd(dev, 0x44U);
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_data(dev, (uint8_t)(((w - 1U) >> 3) & 0xFFU));
    epd_bus_write_cmd(dev, 0x45U);
    epd_bus_write_data(dev, (uint8_t)(y0 & 0xFFU));
    epd_bus_write_data(dev, (uint8_t)((y0 >> 8) & 0xFFU));
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_data(dev, 0x00U);
}

static void set_cursor(epd_t *dev)
{
    const uint16_t y0 = (uint16_t)(dev->panel->height - 1U);

    epd_bus_write_cmd(dev, 0x4EU);
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_cmd(dev, 0x4FU);
    epd_bus_write_data(dev, (uint8_t)(y0 & 0xFFU));
    epd_bus_write_data(dev, (uint8_t)((y0 >> 8) & 0xFFU));
}

static void prepare_ram_write(epd_t *dev)
{
    set_windows(dev);
    set_cursor(dev);
}

static epd_status_t set_lut(epd_t *dev, const uint8_t *lut)
{
    epd_status_t st;
    uint16_t i;

    epd_bus_write_cmd(dev, 0x32U);
    for (i = 0U; i < 153U; i++) {
        epd_bus_write_data(dev, lut[i]);
    }
    st = epd_wait_idle(dev);
    if (st != EPD_OK) {
        return st;
    }

    epd_bus_write_cmd(dev, 0x3FU);
    epd_bus_write_data(dev, lut[153]);
    epd_bus_write_cmd(dev, 0x03U);
    epd_bus_write_data(dev, lut[154]);
    epd_bus_write_cmd(dev, 0x04U);
    epd_bus_write_data(dev, lut[155]);
    epd_bus_write_data(dev, lut[156]);
    epd_bus_write_data(dev, lut[157]);
    epd_bus_write_cmd(dev, 0x2CU);
    epd_bus_write_data(dev, lut[158]);
    return EPD_OK;
}

static epd_status_t panel_init(epd_t *dev)
{
    epd_status_t st;

    epd_bus_hw_reset(dev, 2U, 200U);
    st = epd_wait_idle(dev);
    if (st != EPD_OK) {
        return st;
    }

    epd_bus_write_cmd(dev, 0x12U);
    st = epd_wait_idle(dev);
    if (st != EPD_OK) {
        return st;
    }

    epd_bus_write_cmd(dev, 0x01U);
    epd_bus_write_data(dev, 0xC7U);
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_data(dev, 0x01U);

    epd_bus_write_cmd(dev, 0x11U);
    epd_bus_write_data(dev, 0x01U);

    set_windows(dev);

    epd_bus_write_cmd(dev, 0x3CU);
    epd_bus_write_data(dev, 0x01U);

    epd_bus_write_cmd(dev, 0x18U);
    epd_bus_write_data(dev, 0x80U);

    epd_bus_write_cmd(dev, 0x22U);
    epd_bus_write_data(dev, 0xB1U);
    epd_bus_write_cmd(dev, 0x20U);

    set_cursor(dev);
    st = epd_wait_idle(dev);
    if (st != EPD_OK) {
        return st;
    }

    return set_lut(dev, s_lut_full);
}

static epd_status_t panel_clear(epd_t *dev)
{
    const uint32_t plane = epd_panel_plane_size(dev->panel);

    prepare_ram_write(dev);
    epd_bus_write_plane_fill(dev, 0x24U, 0xFFU, plane);
    prepare_ram_write(dev);
    epd_bus_write_plane_fill(dev, 0x26U, 0xFFU, plane);
    return epd_bus_turn_on_display(dev, UPDATE_FULL);
}

static epd_status_t panel_display(epd_t *dev, const uint8_t *bw, const uint8_t *red)
{
    const uint32_t plane = epd_panel_plane_size(dev->panel);

    (void)red;
    prepare_ram_write(dev);
    epd_bus_write_plane(dev, 0x24U, bw, plane, false);
    return epd_bus_turn_on_display(dev, UPDATE_FULL);
}

static epd_status_t panel_sleep(epd_t *dev)
{
    epd_bus_write_cmd(dev, 0x10U);
    epd_bus_write_data(dev, 0x01U);
    epd_bus_delay_ms(dev, 100U);
    return EPD_OK;
}

const epd_panel_info_t epd_panel_ws_1in54_v2 = {
    .name = "WS_1IN54_V2",
    .width = 200U,
    .height = 200U,
    .planes = 1U,
    .data_entry = 0x01U,
    .update_full = UPDATE_FULL,
    .flip_y = false,
    .init = panel_init,
    .clear = panel_clear,
    .display = panel_display,
    .sleep = panel_sleep,
};
