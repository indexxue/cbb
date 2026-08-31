/**
 * @file    epd_panel_ws_2in13_v3.c
 * @brief   Waveshare 2.13" e-Paper V3（122×250 BW / SSD1680）。
 */

#include "epd_panel.h"

#include <stddef.h>

#define LUT_SIZE      159U
#define UPDATE_FULL   0xC7U

static const uint8_t s_lut_ws_20_30[LUT_SIZE] = {
    0x80, 0x4A, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x4A, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x4A, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x4A, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0F, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x02,
    0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00,
    0x22, 0x17, 0x41, 0x00, 0x32, 0x36,
};

static void set_windows(epd_t *dev)
{
    const uint16_t x1 = (uint16_t)(dev->panel->width - 1U);
    const uint16_t y1 = (uint16_t)(dev->panel->height - 1U);

    epd_bus_write_cmd(dev, 0x44U);
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_data(dev, (uint8_t)((x1 >> 3) & 0xFFU));
    epd_bus_write_cmd(dev, 0x45U);
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_data(dev, (uint8_t)(y1 & 0xFFU));
    epd_bus_write_data(dev, (uint8_t)((y1 >> 8) & 0xFFU));
}

static void set_cursor(epd_t *dev)
{
    epd_bus_write_cmd(dev, 0x4EU);
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_cmd(dev, 0x4FU);
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_data(dev, 0x00U);
}

static void prepare_ram_write(epd_t *dev)
{
    set_windows(dev);
    set_cursor(dev);
}

static epd_status_t lut_by_host(epd_t *dev, const uint8_t *lut)
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

    epd_bus_hw_reset(dev, 2U, 20U);
    epd_bus_delay_ms(dev, 100U);

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
    epd_bus_write_data(dev, 0xF9U);
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_data(dev, 0x00U);

    epd_bus_write_cmd(dev, 0x11U);
    epd_bus_write_data(dev, 0x03U);

    set_windows(dev);
    set_cursor(dev);

    epd_bus_write_cmd(dev, 0x3CU);
    epd_bus_write_data(dev, 0x05U);

    epd_bus_write_cmd(dev, 0x21U);
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_data(dev, 0x80U);

    epd_bus_write_cmd(dev, 0x18U);
    epd_bus_write_data(dev, 0x80U);

    st = epd_wait_idle(dev);
    if (st != EPD_OK) {
        return st;
    }

    return lut_by_host(dev, s_lut_ws_20_30);
}

static epd_status_t panel_clear(epd_t *dev)
{
    const uint32_t plane = epd_panel_plane_size(dev->panel);

    prepare_ram_write(dev);
    epd_bus_write_plane_fill(dev, 0x24U, 0xFFU, plane);
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

const epd_panel_info_t epd_panel_ws_2in13_v3 = {
    .name = "WS_2IN13_V3",
    .width = 122U,
    .height = 250U,
    .planes = 1U,
    .data_entry = 0x03U,
    .update_full = UPDATE_FULL,
    .flip_y = false,
    .init = panel_init,
    .clear = panel_clear,
    .display = panel_display,
    .sleep = panel_sleep,
};
