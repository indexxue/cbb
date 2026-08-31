/**
 * @file    epd_panel_ws_1in54b_v2.c
 * @brief   Waveshare 1.54" e-Paper (B) V2（200×200 BWR / SSD1680）。
 *
 * 对齐官方 EPD_1in54b_V2：data entry 0x01、全刷 0x22=0xF7；
 * 清屏红层填 0x00；Display 时红缓冲按本驱动逻辑 1=红直写（官方对 Paint
 * 「0=红」缓冲取反，与本逻辑等价）。
 */

#include "epd_panel.h"

#include <stddef.h>

#define UPDATE_FULL  0xF7U

static void set_windows(epd_t *dev)
{
    const uint16_t y0 = (uint16_t)(dev->panel->height - 1U);

    epd_bus_write_cmd(dev, 0x44U);
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_data(dev, 0x18U); /* (24+1)*8 = 200 */
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

    epd_bus_write_cmd(dev, 0x01U);
    epd_bus_write_data(dev, 0xC7U);
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_data(dev, 0x01U); /* V2 BWR：第三字节为 0x01 */

    epd_bus_write_cmd(dev, 0x11U);
    epd_bus_write_data(dev, 0x01U);

    set_windows(dev);

    epd_bus_write_cmd(dev, 0x3CU);
    epd_bus_write_data(dev, 0x05U);

    epd_bus_write_cmd(dev, 0x18U);
    epd_bus_write_data(dev, 0x80U);

    set_cursor(dev);
    return epd_wait_idle(dev);
}

static epd_status_t panel_clear(epd_t *dev)
{
    const uint32_t plane = epd_panel_plane_size(dev->panel);
    const uint8_t white = dev->bw_invert ? 0x00U : 0xFFU;

    prepare_ram_write(dev);
    epd_bus_write_plane_fill(dev, 0x24U, white, plane);
    epd_bus_write_plane_fill(dev, 0x26U, 0x00U, plane);
    return epd_bus_turn_on_display(dev, UPDATE_FULL);
}

static epd_status_t panel_display(epd_t *dev, const uint8_t *bw, const uint8_t *red)
{
    const uint32_t plane = epd_panel_plane_size(dev->panel);

    prepare_ram_write(dev);
    epd_bus_write_plane(dev, 0x24U, bw, plane, dev->bw_invert);

    if (red != NULL) {
        /* 逻辑 1=红 → RAM 1=红（官方对「0=红」Paint 缓冲取反） */
        epd_bus_write_plane(dev, 0x26U, red, plane, false);
    } else {
        epd_bus_write_plane_fill(dev, 0x26U, 0x00U, plane);
    }

    return epd_bus_turn_on_display(dev, UPDATE_FULL);
}

static epd_status_t panel_sleep(epd_t *dev)
{
    epd_bus_write_cmd(dev, 0x10U);
    epd_bus_write_data(dev, 0x01U);
    epd_bus_delay_ms(dev, 100U);
    return EPD_OK;
}

const epd_panel_info_t epd_panel_ws_1in54b_v2 = {
    .name = "WS_1IN54B_V2",
    .width = 200U,
    .height = 200U,
    .planes = 2U,
    .data_entry = 0x01U,
    .update_full = UPDATE_FULL,
    .flip_y = true,
    .init = panel_init,
    .clear = panel_clear,
    .display = panel_display,
    .sleep = panel_sleep,
};
