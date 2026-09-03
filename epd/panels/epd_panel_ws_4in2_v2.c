/**
 * @file    epd_panel_ws_4in2_v2.c
 * @brief   Waveshare 4.2" e-Paper Module V2（400×300 BW / SSD1683）。
 *
 * 对齐官方 EPD_4in2_V2：data entry 0x03、全刷 0xF7；双 RAM 同图。
 */

#include "epd_panel.h"

#include <stddef.h>

#define UPDATE_FULL  0xF7U

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

static epd_status_t panel_init(epd_t *dev)
{
    epd_status_t st;

    /* 官方：高 100ms → 低 2ms → 高 100ms */
    epd_bus_hw_reset(dev, 2U, 100U);

    st = epd_wait_idle(dev);
    if (st != EPD_OK) {
        return st;
    }

    epd_bus_write_cmd(dev, 0x12U);
    st = epd_wait_idle(dev);
    if (st != EPD_OK) {
        return st;
    }

    epd_bus_write_cmd(dev, 0x21U);
    epd_bus_write_data(dev, 0x40U);
    epd_bus_write_data(dev, 0x00U);

    epd_bus_write_cmd(dev, 0x3CU);
    epd_bus_write_data(dev, 0x05U);

    epd_bus_write_cmd(dev, 0x11U);
    epd_bus_write_data(dev, 0x03U);

    set_windows(dev);
    set_cursor(dev);
    return epd_wait_idle(dev);
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
    epd_bus_write_plane(dev, 0x24U, bw, plane, dev->bw_invert);
    prepare_ram_write(dev);
    epd_bus_write_plane(dev, 0x26U, bw, plane, dev->bw_invert);
    return epd_bus_turn_on_display(dev, UPDATE_FULL);
}

static epd_status_t panel_sleep(epd_t *dev)
{
    epd_bus_write_cmd(dev, 0x10U);
    epd_bus_write_data(dev, 0x01U);
    epd_bus_delay_ms(dev, 200U);
    return EPD_OK;
}

const epd_panel_info_t epd_panel_ws_4in2_v2 = {
    .name = "WS_4IN2_V2",
    .width = 400U,
    .height = 300U,
    .planes = 1U,
    .data_entry = 0x03U,
    .update_full = UPDATE_FULL,
    .flip_y = false,
    .init = panel_init,
    .clear = panel_clear,
    .display = panel_display,
    .sleep = panel_sleep,
};
