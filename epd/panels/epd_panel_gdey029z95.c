/**
 * @file    epd_panel_gdey029z95.c
 * @brief   GoodDisplay S-GDEY029Z95（BWR）与同 OTP 时序的 2.9" 纯黑白 panel。
 */

#include "epd_panel.h"

#include <stddef.h>

#define UPDATE_FULL  0xF7U

static void hw_reset(epd_t *dev)
{
    if (dev->set_rst == NULL) {
        return;
    }
    /* GoodDisplay：RST 拉低 ≥10ms 后释放，再等 ≥10ms */
    dev->set_rst(0);
    epd_bus_delay_ms(dev, 10U);
    dev->set_rst(1);
    epd_bus_delay_ms(dev, 10U);
}

static void set_windows(epd_t *dev)
{
    const uint16_t y_end = (uint16_t)(dev->panel->height - 1U);
    const uint16_t row_bytes = epd_panel_bytes_per_row(dev->panel);

    epd_bus_write_cmd(dev, 0x44U);
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_data(dev, (uint8_t)(row_bytes - 1U));
    epd_bus_write_cmd(dev, 0x45U);
    epd_bus_write_data(dev, (uint8_t)(y_end & 0xFFU));
    epd_bus_write_data(dev, (uint8_t)((y_end >> 8) & 0xFFU));
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_data(dev, 0x00U);
}

static void set_cursor(epd_t *dev)
{
    const uint16_t y_end = (uint16_t)(dev->panel->height - 1U);

    epd_bus_write_cmd(dev, 0x4EU);
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_cmd(dev, 0x4FU);
    epd_bus_write_data(dev, (uint8_t)(y_end & 0xFFU));
    epd_bus_write_data(dev, (uint8_t)((y_end >> 8) & 0xFFU));
}

static void prepare_ram_write(epd_t *dev)
{
    set_windows(dev);
    set_cursor(dev);
}

static epd_status_t panel_init(epd_t *dev)
{
    epd_status_t st;
    const uint16_t height = dev->panel->height;

    hw_reset(dev);
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
    epd_bus_write_data(dev, (uint8_t)((height - 1U) & 0xFFU));
    epd_bus_write_data(dev, (uint8_t)(((height - 1U) >> 8) & 0xFFU));
    epd_bus_write_data(dev, 0x00U);

    epd_bus_write_cmd(dev, 0x11U);
    epd_bus_write_data(dev, 0x01U);

    set_windows(dev);

    epd_bus_write_cmd(dev, 0x3CU);
    epd_bus_write_data(dev, 0x05U);

    epd_bus_write_cmd(dev, 0x21U);
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_data(dev, 0x80U);

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
        epd_bus_write_plane(dev, 0x26U, red, plane, false);
    } else {
        /* mono：红层填 0（无红） */
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

const epd_panel_info_t epd_panel_gdey029z95 = {
    .name = "GDEY029Z95",
    .width = 128U,
    .height = 296U,
    .planes = 2U,
    .data_entry = 0x01U,
    .update_full = UPDATE_FULL,
    .flip_y = true,
    .init = panel_init,
    .clear = panel_clear,
    .display = panel_display,
    .sleep = panel_sleep,
};

/**
 * 同 SSD1680 OTP 时序的 2.9" 纯黑白：init/窗口与 GDEY029Z95 相同，
 * 但不声明红平面（红 RAM 在 display 时填 0）。VET6 实测无红墨水用此 profile。
 */
const epd_panel_info_t epd_panel_2in9_bw_otp = {
    .name = "2IN9_BW_OTP",
    .width = 128U,
    .height = 296U,
    .planes = 1U,
    .data_entry = 0x01U,
    .update_full = UPDATE_FULL,
    .flip_y = true,
    .init = panel_init,
    .clear = panel_clear,
    .display = panel_display,
    .sleep = panel_sleep,
};
