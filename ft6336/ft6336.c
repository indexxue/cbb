/**
 * @file    ft6336.c
 * @brief   FT6336 TD_STATUS + P1/P2 寄存器读点。
 */

#include "ft6336.h"

#include <stddef.h>

#define FT6336_REG_TD_STATUS 0x02U
#define FT6336_REG_P1_XH 0x03U
#define FT6336_REG_CHIP_ID 0xA3U

#define FT6336_EVENT_PUT_DOWN 0x00U
#define FT6336_EVENT_CONTACT 0x02U

/** 复位后固件启动等待（数据手册 / 模组常见 200 ms 量级） */
#define FT6336_POST_RESET_MS 200U

static uint8_t s_last_chip_id;

static ft6336_status_t reg_read(ft6336_t *dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    uint8_t addr = reg;

    if ((dev == NULL) || (dev->write_read == NULL) || (data == NULL) || (len == 0U)) {
        return FT6336_ERROR_PARAM;
    }
    if (dev->write_read(dev->address, &addr, 1U, data, len) != 0) {
        return FT6336_ERROR_I2C;
    }
    return FT6336_OK;
}

static uint16_t map_axis(uint16_t v, uint16_t panel, uint16_t disp)
{
    if ((panel == 0U) || (disp == 0U)) {
        return v;
    }
    if (v >= panel) {
        v = (uint16_t)(panel - 1U);
    }
    return (uint16_t)(((uint32_t)v * (uint32_t)disp) / (uint32_t)panel);
}

static void map_point(const ft6336_t *dev, uint16_t raw_x, uint16_t raw_y, ft6336_point_t *out)
{
    uint16_t x;
    uint16_t y;

    if (dev->swap_xy) {
        uint16_t tmp = raw_x;
        raw_x = raw_y;
        raw_y = tmp;
    }

    x = map_axis(raw_x, dev->panel_w, dev->disp_w);
    y = map_axis(raw_y, dev->panel_h, dev->disp_h);

    if (dev->invert_x && (dev->disp_w > 0U)) {
        x = (uint16_t)(dev->disp_w - 1U - x);
    }
    if (dev->invert_y && (dev->disp_h > 0U)) {
        y = (uint16_t)(dev->disp_h - 1U - y);
    }

    out->x = (int16_t)x;
    out->y = (int16_t)y;
}

static void parse_touch_slot(const uint8_t *raw6, uint8_t *event, uint8_t *id, uint16_t *x, uint16_t *y)
{
    *event = (uint8_t)((raw6[0] >> 6) & 0x03U);
    *id = (uint8_t)((raw6[2] >> 4) & 0x0FU);
    *x = (uint16_t)((((uint16_t)raw6[0] & 0x0FU) << 8) | (uint16_t)raw6[1]);
    *y = (uint16_t)((((uint16_t)raw6[2] & 0x0FU) << 8) | (uint16_t)raw6[3]);
}

static bool event_is_active(uint8_t event)
{
    return (event == FT6336_EVENT_PUT_DOWN) || (event == FT6336_EVENT_CONTACT);
}

static bool chip_id_accepted(uint8_t id)
{
    return (id == FT6336_CHIP_ID_EXPECT) || (id == FT6336_CHIP_ID_FT6236) ||
           (id == FT6336_CHIP_ID_FT6206);
}

static void hw_reset(ft6336_t *dev)
{
    if ((dev->set_rst == NULL) || (dev->delay_ms == NULL)) {
        if (dev->delay_ms != NULL) {
            dev->delay_ms(FT6336_POST_RESET_MS);
        }
        return;
    }
    dev->set_rst(1);
    dev->delay_ms(10U);
    dev->set_rst(0);
    dev->delay_ms(10U);
    dev->set_rst(1);
    dev->delay_ms(FT6336_POST_RESET_MS);
}

uint8_t ft6336_chip_id(const ft6336_t *dev)
{
    (void)dev;
    return s_last_chip_id;
}

ft6336_status_t ft6336_init(ft6336_t *dev, const ft6336_config_t *cfg)
{
    uint8_t chip_id = 0U;
    ft6336_status_t st;

    if ((dev == NULL) || (cfg == NULL) || (cfg->write_read == NULL)) {
        return FT6336_ERROR_PARAM;
    }

    dev->write_read = cfg->write_read;
    dev->set_rst = cfg->set_rst;
    dev->delay_ms = cfg->delay_ms;
    dev->address = (cfg->address != 0U) ? cfg->address : (uint8_t)FT6336_I2C_ADDR_DEFAULT;
    dev->panel_w = (cfg->panel_w != 0U) ? cfg->panel_w : 320U;
    dev->panel_h = (cfg->panel_h != 0U) ? cfg->panel_h : 480U;
    dev->disp_w = (cfg->disp_w != 0U) ? cfg->disp_w : dev->panel_w;
    dev->disp_h = (cfg->disp_h != 0U) ? cfg->disp_h : dev->panel_h;
    dev->swap_xy = cfg->swap_xy;
    dev->invert_x = cfg->invert_x;
    dev->invert_y = cfg->invert_y;
    dev->initialized = false;
    s_last_chip_id = 0U;

    hw_reset(dev);

    st = reg_read(dev, FT6336_REG_CHIP_ID, &chip_id, 1U);
    if (st != FT6336_OK) {
        return st;
    }
    s_last_chip_id = chip_id;
    if (!chip_id_accepted(chip_id)) {
        return FT6336_ERROR_ID;
    }

    dev->initialized = true;
    return FT6336_OK;
}

ft6336_status_t ft6336_read_point(ft6336_t *dev, ft6336_point_t *out)
{
    uint8_t count = 0U;
    ft6336_status_t st;

    if ((dev == NULL) || !dev->initialized || (out == NULL)) {
        return FT6336_ERROR_NOT_INIT;
    }

    st = ft6336_read_points(dev, out, 1U, &count);
    if (st != FT6336_OK) {
        out->pressed = false;
        return st;
    }
    return FT6336_OK;
}

ft6336_status_t ft6336_read_points(ft6336_t *dev,
                                   ft6336_point_t *out,
                                   uint8_t max_points,
                                   uint8_t *out_count)
{
    uint8_t td = 0U;
    uint8_t raw[12];
    ft6336_status_t st;
    uint8_t n;
    uint8_t filled;
    uint8_t i;

    if ((dev == NULL) || !dev->initialized) {
        return FT6336_ERROR_NOT_INIT;
    }
    if ((out == NULL) || (out_count == NULL) || (max_points == 0U)) {
        return FT6336_ERROR_PARAM;
    }

    *out_count = 0U;
    if (max_points > FT6336_MAX_POINTS) {
        max_points = (uint8_t)FT6336_MAX_POINTS;
    }

    st = reg_read(dev, FT6336_REG_TD_STATUS, &td, 1U);
    if (st != FT6336_OK) {
        return st;
    }

    n = (uint8_t)(td & 0x0FU);
    if (n == 0U) {
        return FT6336_ERROR_NO_POINT;
    }
    if (n > FT6336_MAX_POINTS) {
        n = (uint8_t)FT6336_MAX_POINTS;
    }

    st = reg_read(dev, FT6336_REG_P1_XH, raw, (uint16_t)(n * 6U));
    if (st != FT6336_OK) {
        return st;
    }

    filled = 0U;
    for (i = 0U; (i < n) && (filled < max_points); i++) {
        uint8_t event;
        uint8_t id;
        uint16_t raw_x;
        uint16_t raw_y;
        const uint8_t *slot = &raw[i * 6U];

        parse_touch_slot(slot, &event, &id, &raw_x, &raw_y);
        if (!event_is_active(event)) {
            continue;
        }

        map_point(dev, raw_x, raw_y, &out[filled]);
        out[filled].id = id;
        out[filled].pressed = true;
        filled++;
    }

    *out_count = filled;
    if (filled == 0U) {
        return FT6336_ERROR_NO_POINT;
    }
    return FT6336_OK;
}

bool ft6336_is_initialized(const ft6336_t *dev)
{
    return (dev != NULL) && dev->initialized;
}
