/**
 * @file    it7259.c
 * @brief   IT7259 Query Buffer + Point Information Buffer 读点。
 */

#include "it7259.h"

#include <stddef.h>

static it7259_status_t buf_read(it7259_t *dev, uint8_t buf_idx, uint8_t *data, uint16_t len)
{
    uint8_t idx = buf_idx;

    if ((dev == NULL) || (dev->write_read == NULL) || (data == NULL) || (len == 0U)) {
        return IT7259_ERROR_PARAM;
    }
    if (dev->write_read(dev->address, &idx, 1U, data, len) != 0) {
        return IT7259_ERROR_I2C;
    }
    return IT7259_OK;
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

it7259_status_t it7259_init(it7259_t *dev, const it7259_config_t *cfg)
{
    if ((dev == NULL) || (cfg == NULL) || (cfg->write_read == NULL)) {
        return IT7259_ERROR_PARAM;
    }

    dev->write_read = cfg->write_read;
    dev->delay_ms = cfg->delay_ms;
    dev->address = (cfg->address != 0U) ? cfg->address : (uint8_t)IT7259_I2C_ADDR_DEFAULT;
    dev->panel_w = (cfg->panel_w != 0U) ? cfg->panel_w : 240U;
    dev->panel_h = (cfg->panel_h != 0U) ? cfg->panel_h : 240U;
    dev->disp_w = (cfg->disp_w != 0U) ? cfg->disp_w : 240U;
    dev->disp_h = (cfg->disp_h != 0U) ? cfg->disp_h : 240U;
    dev->swap_xy = cfg->swap_xy;
    dev->invert_x = cfg->invert_x;
    dev->invert_y = cfg->invert_y;
    dev->initialized = true;
    return IT7259_OK;
}

it7259_status_t it7259_read_point(it7259_t *dev, it7259_point_t *out)
{
    uint8_t query = 0U;
    uint8_t pt[14];
    it7259_status_t st;
    uint16_t raw_x;
    uint16_t raw_y;
    uint16_t x;
    uint16_t y;
    bool finger0;

    if ((dev == NULL) || !dev->initialized || (out == NULL)) {
        return IT7259_ERROR_NOT_INIT;
    }

    st = buf_read(dev, IT7259_BUF_QUERY, &query, 1U);
    if (st != IT7259_OK) {
        /* busy/NACK：不更新，由上层保持上次状态 */
        return IT7259_ERROR_BUSY;
    }

    if ((query & IT7259_QUERY_POINT) == 0U) {
        out->pressed = false;
        /* 保持 out->x/y 由调用方沿用上次坐标，避免抬起跳到 (0,0) 导致 CLICK 失败 */
        return IT7259_ERROR_NO_POINT;
    }

    st = buf_read(dev, IT7259_BUF_POINT, pt, (uint16_t)sizeof(pt));
    if (st != IT7259_OK) {
        return st;
    }

    /* bit0..2 手指掩码；部分固件也用 0x08 表示点信息 */
    finger0 = ((pt[0] & 0x07U) != 0U) || ((pt[0] & 0x08U) != 0U);
    raw_x = (uint16_t)pt[2] | (uint16_t)(((uint16_t)pt[3] & 0x0FU) << 8);
    raw_y = (uint16_t)pt[4] | (uint16_t)(((uint16_t)pt[3] & 0xF0U) << 4);

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

    out->pressed = finger0;
    out->x = (int16_t)x;
    out->y = (int16_t)y;
    return IT7259_OK;
}

bool it7259_is_initialized(const it7259_t *dev)
{
    return (dev != NULL) && dev->initialized;
}
