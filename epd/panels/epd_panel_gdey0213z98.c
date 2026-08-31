/**
 * @file    epd_panel_gdey0213z98.c
 * @brief   GDEY0213Z98 / 122×250 BWR 两套 init（OTP Y递减 + GxEPD Y递增）。
 */

#include "epd_panel.h"

#include <stddef.h>

#define UPDATE_FULL  0xF7U

/* ---------- 公共 ---------- */

static void hw_reset_short(epd_t *dev)
{
    if (dev->set_rst == NULL) {
        return;
    }
    dev->set_rst(0);
    epd_bus_delay_ms(dev, 10U);
    dev->set_rst(1);
    epd_bus_delay_ms(dev, 10U);
}

static epd_status_t turn_on_f7(epd_t *dev)
{
    return epd_bus_turn_on_display(dev, UPDATE_FULL);
}

static epd_status_t panel_sleep(epd_t *dev)
{
    epd_bus_write_cmd(dev, 0x10U);
    epd_bus_write_data(dev, 0x01U);
    epd_bus_delay_ms(dev, 100U);
    return EPD_OK;
}

/* ---------- A: GDEY029 同族 OTP（data entry 0x01，Y 递减） ---------- */

static void set_windows_ydec(epd_t *dev)
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

static void set_cursor_ydec(epd_t *dev)
{
    const uint16_t y_end = (uint16_t)(dev->panel->height - 1U);

    epd_bus_write_cmd(dev, 0x4EU);
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_cmd(dev, 0x4FU);
    epd_bus_write_data(dev, (uint8_t)(y_end & 0xFFU));
    epd_bus_write_data(dev, (uint8_t)((y_end >> 8) & 0xFFU));
}

static epd_status_t otp_ydec_init(epd_t *dev)
{
    epd_status_t st;
    const uint16_t height = dev->panel->height;

    hw_reset_short(dev);
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

    set_windows_ydec(dev);

    epd_bus_write_cmd(dev, 0x3CU);
    epd_bus_write_data(dev, 0x05U);

    epd_bus_write_cmd(dev, 0x21U);
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_data(dev, 0x80U);

    epd_bus_write_cmd(dev, 0x18U);
    epd_bus_write_data(dev, 0x80U);

    set_cursor_ydec(dev);
    return epd_wait_idle(dev);
}

static epd_status_t otp_ydec_clear(epd_t *dev)
{
    const uint32_t plane = epd_panel_plane_size(dev->panel);
    const uint8_t white = dev->bw_invert ? 0x00U : 0xFFU;

    set_windows_ydec(dev);
    set_cursor_ydec(dev);
    epd_bus_write_plane_fill(dev, 0x24U, white, plane);
    epd_bus_write_plane_fill(dev, 0x26U, 0x00U, plane);
    return turn_on_f7(dev);
}

static epd_status_t otp_ydec_display(epd_t *dev, const uint8_t *bw, const uint8_t *red)
{
    const uint32_t plane = epd_panel_plane_size(dev->panel);

    set_windows_ydec(dev);
    set_cursor_ydec(dev);
    epd_bus_write_plane(dev, 0x24U, bw, plane, dev->bw_invert);
    if (red != NULL) {
        epd_bus_write_plane(dev, 0x26U, red, plane, false);
    } else {
        epd_bus_write_plane_fill(dev, 0x26U, 0x00U, plane);
    }
    return turn_on_f7(dev);
}

const epd_panel_info_t epd_panel_gdey0213z98 = {
    .name = "GDEY0213Z98",
    .width = 122U,
    .height = 250U,
    .planes = 2U,
    .data_entry = 0x01U,
    .update_full = UPDATE_FULL,
    .flip_y = true,
    .init = otp_ydec_init,
    .clear = otp_ydec_clear,
    .display = otp_ydec_display,
    .sleep = panel_sleep,
};

/* ---------- B: GxEPD2_213_Z98c（data entry 0x03，Y 递增，0x22=0xF7） ---------- */

static void set_windows_yinc(epd_t *dev)
{
    const uint16_t y1 = (uint16_t)(dev->panel->height - 1U);
    const uint16_t row_bytes = epd_panel_bytes_per_row(dev->panel);

    epd_bus_write_cmd(dev, 0x44U);
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_data(dev, (uint8_t)(row_bytes - 1U));
    epd_bus_write_cmd(dev, 0x45U);
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_data(dev, (uint8_t)(y1 & 0xFFU));
    epd_bus_write_data(dev, (uint8_t)((y1 >> 8) & 0xFFU));
}

static void set_cursor_yinc(epd_t *dev)
{
    epd_bus_write_cmd(dev, 0x4EU);
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_cmd(dev, 0x4FU);
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_data(dev, 0x00U);
}

static epd_status_t gx_yinc_init(epd_t *dev)
{
    epd_status_t st;
    const uint16_t height = dev->panel->height;

    epd_bus_hw_reset(dev, 10U, 10U);

    st = epd_wait_idle(dev);
    if (st != EPD_OK) {
        return st;
    }

    epd_bus_write_cmd(dev, 0x12U);
    epd_bus_delay_ms(dev, 10U);
    st = epd_wait_idle(dev);
    if (st != EPD_OK) {
        return st;
    }

    epd_bus_write_cmd(dev, 0x01U);
    epd_bus_write_data(dev, (uint8_t)((height - 1U) & 0xFFU));
    epd_bus_write_data(dev, (uint8_t)(((height - 1U) >> 8) & 0xFFU));
    epd_bus_write_data(dev, 0x00U);

    epd_bus_write_cmd(dev, 0x11U);
    epd_bus_write_data(dev, 0x03U);

    epd_bus_write_cmd(dev, 0x3CU);
    epd_bus_write_data(dev, 0x05U);

    epd_bus_write_cmd(dev, 0x18U);
    epd_bus_write_data(dev, 0x80U);

    epd_bus_write_cmd(dev, 0x21U);
    epd_bus_write_data(dev, 0x00U);
    epd_bus_write_data(dev, 0x80U);

    set_windows_yinc(dev);
    set_cursor_yinc(dev);
    return epd_wait_idle(dev);
}

static epd_status_t gx_yinc_clear(epd_t *dev)
{
    const uint32_t plane = epd_panel_plane_size(dev->panel);
    const uint8_t white = dev->bw_invert ? 0x00U : 0xFFU;

    set_windows_yinc(dev);
    set_cursor_yinc(dev);
    epd_bus_write_plane_fill(dev, 0x24U, white, plane);
    epd_bus_write_plane_fill(dev, 0x26U, 0x00U, plane);
    return turn_on_f7(dev);
}

static epd_status_t gx_yinc_display(epd_t *dev, const uint8_t *bw, const uint8_t *red)
{
    const uint32_t plane = epd_panel_plane_size(dev->panel);

    set_windows_yinc(dev);
    set_cursor_yinc(dev);
    epd_bus_write_plane(dev, 0x24U, bw, plane, dev->bw_invert);
    if (red != NULL) {
        /* GxEPD 对「0=色」取反后 RAM 1=红；本逻辑 1=红 → 直写 */
        epd_bus_write_plane(dev, 0x26U, red, plane, false);
    } else {
        epd_bus_write_plane_fill(dev, 0x26U, 0x00U, plane);
    }
    return turn_on_f7(dev);
}

const epd_panel_info_t epd_panel_gdey0213z98_gx = {
    .name = "GDEY0213Z98_GX",
    .width = 122U,
    .height = 250U,
    .planes = 2U,
    .data_entry = 0x03U,
    .update_full = UPDATE_FULL,
    .flip_y = false,
    .init = gx_yinc_init,
    .clear = gx_yinc_clear,
    .display = gx_yinc_display,
    .sleep = panel_sleep,
};
