/**
 * @file    epd_panel_ws_2in13b_v4.c
 * @brief   Waveshare 2.13" e-Paper (B) V4（122×250 BWR / SSD1680）。
 *
 * 对齐官方 EPD_2in13b_V4：data entry 0x03、0x21=0x80/0x80、
 * 全刷仅 0x20（不写 0x22）；红 RAM 0=红 / 0xFF=无红（写时对逻辑缓冲取反）。
 */

#include "epd_panel.h"

#include <stddef.h>

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

static epd_status_t turn_on_display(epd_t *dev)
{
    /* Waveshare V4：只 Activate，不写 0x22 */
    epd_bus_write_cmd(dev, 0x20U);
    if (dev->read_busy != NULL) {
        return epd_wait_idle(dev);
    }
    /* BUSY 不可用：BWR 全刷约 15s */
    epd_bus_delay_ms(dev, 16000U);
    return EPD_OK;
}

static epd_status_t panel_init(epd_t *dev)
{
    epd_status_t st;

    /* 官方：RST 高 20ms → 低 2ms → 高 20ms */
    epd_bus_hw_reset(dev, 2U, 20U);

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

    epd_bus_write_cmd(dev, 0x18U);
    epd_bus_write_data(dev, 0x80U);

    /* 与 GDEY/GxEPD 的 0x00,0x80 不同：V4 官方为 0x80,0x80 */
    epd_bus_write_cmd(dev, 0x21U);
    epd_bus_write_data(dev, 0x80U);
    epd_bus_write_data(dev, 0x80U);

    return epd_wait_idle(dev);
}

static epd_status_t panel_clear(epd_t *dev)
{
    const uint32_t plane = epd_panel_plane_size(dev->panel);

    epd_bus_write_plane_fill(dev, 0x24U, 0xFFU, plane);
    epd_bus_write_plane_fill(dev, 0x26U, 0xFFU, plane);
    return turn_on_display(dev);
}

static epd_status_t panel_display(epd_t *dev, const uint8_t *bw, const uint8_t *red)
{
    const uint32_t plane = epd_panel_plane_size(dev->panel);

    epd_bus_write_plane(dev, 0x24U, bw, plane, dev->bw_invert);

    if (red != NULL) {
        /* 逻辑 1=红 → 控制器 0=红，故取反 */
        epd_bus_write_plane(dev, 0x26U, red, plane, true);
    } else {
        epd_bus_write_plane_fill(dev, 0x26U, 0xFFU, plane);
    }

    return turn_on_display(dev);
}

static epd_status_t panel_sleep(epd_t *dev)
{
    epd_bus_write_cmd(dev, 0x10U);
    epd_bus_write_data(dev, 0x01U);
    epd_bus_delay_ms(dev, 100U);
    return EPD_OK;
}

const epd_panel_info_t epd_panel_ws_2in13b_v4 = {
    .name = "WS_2IN13B_V4",
    .width = 122U,
    .height = 250U,
    .planes = 2U,
    .data_entry = 0x03U,
    .update_full = 0x00U, /* 本 panel 自管 0x20，不经 0x22 */
    .flip_y = false,
    .init = panel_init,
    .clear = panel_clear,
    .display = panel_display,
    .sleep = panel_sleep,
};
