/**
 * @file    epd.c
 * @brief   SSD16xx 墨水屏共享总线与 panel 调度。
 */

#include "epd.h"
#include "epd_panel.h"

#include <stddef.h>

#define EPD_BUSY_TIMEOUT_MS  8000U

static bool is_busy(const epd_t *dev)
{
    int level;

    if (dev->read_busy == NULL) {
        return false;
    }

    level = dev->read_busy();
    if (dev->busy_active_high) {
        return level != 0;
    }
    return level == 0;
}

void epd_bus_delay_ms(epd_t *dev, uint32_t ms)
{
    if (dev != NULL && dev->delay_ms != NULL) {
        dev->delay_ms(ms);
    }
}

static void cs_low(epd_t *dev)
{
    if (dev->set_cs != NULL) {
        dev->set_cs(0);
    }
}

static void cs_high(epd_t *dev)
{
    if (dev->set_cs != NULL) {
        dev->set_cs(1);
    }
}

void epd_bus_hw_reset(epd_t *dev, uint32_t hold_ms, uint32_t settle_ms)
{
    if (dev->set_rst == NULL) {
        return;
    }

    dev->set_rst(1);
    epd_bus_delay_ms(dev, settle_ms);
    dev->set_rst(0);
    epd_bus_delay_ms(dev, hold_ms);
    dev->set_rst(1);
    epd_bus_delay_ms(dev, settle_ms);
}

void epd_bus_write_cmd(epd_t *dev, uint8_t cmd)
{
    if (dev->set_dc != NULL) {
        dev->set_dc(0);
    }
    cs_low(dev);
    if (dev->spi_tx != NULL) {
        dev->spi_tx(&cmd, 1U);
    }
    cs_high(dev);
}

void epd_bus_write_data(epd_t *dev, uint8_t data)
{
    if (dev->set_dc != NULL) {
        dev->set_dc(1);
    }
    cs_low(dev);
    if (dev->spi_tx != NULL) {
        dev->spi_tx(&data, 1U);
    }
    cs_high(dev);
}

void epd_bus_write_data_buf(epd_t *dev, const uint8_t *buf, uint16_t len)
{
    if (len == 0U || buf == NULL || dev->spi_tx == NULL) {
        return;
    }

    if (dev->set_dc != NULL) {
        dev->set_dc(1);
    }
    cs_low(dev);
    dev->spi_tx(buf, len);
    cs_high(dev);
}

epd_status_t epd_bus_turn_on_display(epd_t *dev, uint8_t ctrl)
{
    if (ctrl != 0U) {
        epd_bus_write_cmd(dev, 0x22U);
        epd_bus_write_data(dev, ctrl);
    }
    epd_bus_write_cmd(dev, 0x20U);
    if (dev->read_busy != NULL) {
        return epd_wait_idle(dev);
    }
    /* BUSY 不可用时：BWR 全刷约 15s */
    epd_bus_delay_ms(dev, 16000U);
    return EPD_OK;
}

void epd_bus_write_plane_fill(epd_t *dev, uint8_t cmd, uint8_t fill, uint32_t plane_size)
{
    uint8_t chunk[64];
    uint32_t remain = plane_size;
    uint16_t i;

    for (i = 0U; i < (uint16_t)sizeof(chunk); i++) {
        chunk[i] = fill;
    }

    epd_bus_write_cmd(dev, cmd);
    while (remain > 0U) {
        uint16_t n = (remain > (uint32_t)sizeof(chunk)) ? (uint16_t)sizeof(chunk) : (uint16_t)remain;
        epd_bus_write_data_buf(dev, chunk, n);
        remain -= n;
    }
}

void epd_bus_write_plane(epd_t *dev, uint8_t cmd, const uint8_t *plane, uint32_t plane_size,
                         bool invert)
{
    uint8_t chunk[64];
    uint32_t remain = plane_size;
    uint32_t offset = 0U;

    epd_bus_write_cmd(dev, cmd);
    while (remain > 0U) {
        uint16_t n = (remain > (uint32_t)sizeof(chunk)) ? (uint16_t)sizeof(chunk) : (uint16_t)remain;
        uint16_t i;

        if (!invert) {
            epd_bus_write_data_buf(dev, &plane[offset], n);
        } else {
            for (i = 0U; i < n; i++) {
                chunk[i] = (uint8_t)~plane[offset + i];
            }
            epd_bus_write_data_buf(dev, chunk, n);
        }
        offset += n;
        remain -= n;
    }
}

epd_status_t epd_bus_check(const epd_t *dev, bool require_init)
{
    if (dev == NULL) {
        return EPD_ERROR_PARAM;
    }
    if (dev->spi_tx == NULL || dev->set_dc == NULL || dev->delay_ms == NULL || dev->panel == NULL) {
        return EPD_ERROR_PARAM;
    }
    if (require_init && !dev->initialized) {
        return EPD_ERROR_NOT_INIT;
    }
    return EPD_OK;
}

epd_status_t epd_register(epd_t *dev, const epd_config_t *cfg)
{
    if (dev == NULL || cfg == NULL) {
        return EPD_ERROR_PARAM;
    }
    if (cfg->spi_tx == NULL || cfg->set_dc == NULL || cfg->delay_ms == NULL || cfg->panel == NULL) {
        return EPD_ERROR_PARAM;
    }

    dev->spi_tx = cfg->spi_tx;
    dev->set_cs = cfg->set_cs;
    dev->set_dc = cfg->set_dc;
    dev->set_rst = cfg->set_rst;
    dev->read_busy = cfg->read_busy;
    dev->delay_ms = cfg->delay_ms;
    dev->busy_active_high = cfg->busy_active_high;
    dev->bw_invert = cfg->bw_invert;
    dev->panel = cfg->panel;
    dev->initialized = false;

    return EPD_OK;
}

epd_status_t epd_wait_idle(epd_t *dev)
{
    epd_status_t st = epd_bus_check(dev, false);
    uint32_t waited = 0U;

    if (st != EPD_OK) {
        return st;
    }
    if (dev->read_busy == NULL) {
        return EPD_OK;
    }

    epd_bus_delay_ms(dev, 10U);
    waited += 10U;
    while (is_busy(dev)) {
        epd_bus_delay_ms(dev, 50U);
        waited += 50U;
        if (waited >= EPD_BUSY_TIMEOUT_MS) {
            return EPD_ERROR_BUSY;
        }
    }
    epd_bus_delay_ms(dev, 200U);
    return EPD_OK;
}

epd_status_t epd_init(epd_t *dev)
{
    epd_status_t st = epd_bus_check(dev, false);

    if (st != EPD_OK) {
        return st;
    }
    if (dev->panel->init == NULL) {
        return EPD_ERROR_UNSUPPORTED;
    }

    st = dev->panel->init(dev);
    if (st == EPD_OK) {
        dev->initialized = true;
    }
    return st;
}

epd_status_t epd_clear(epd_t *dev)
{
    epd_status_t st = epd_bus_check(dev, true);

    if (st != EPD_OK) {
        return st;
    }
    if (dev->panel->clear == NULL) {
        return EPD_ERROR_UNSUPPORTED;
    }
    return dev->panel->clear(dev);
}

epd_status_t epd_display(epd_t *dev, const uint8_t *bw, const uint8_t *red)
{
    epd_status_t st = epd_bus_check(dev, true);

    if (st != EPD_OK) {
        return st;
    }
    if (bw == NULL || dev->panel->display == NULL) {
        return (dev->panel->display == NULL) ? EPD_ERROR_UNSUPPORTED : EPD_ERROR_PARAM;
    }
    return dev->panel->display(dev, bw, red);
}

epd_status_t epd_sleep(epd_t *dev)
{
    epd_status_t st = epd_bus_check(dev, true);

    if (st != EPD_OK) {
        return st;
    }
    if (dev->panel->sleep == NULL) {
        return EPD_ERROR_UNSUPPORTED;
    }

    st = dev->panel->sleep(dev);
    if (st == EPD_OK) {
        dev->initialized = false;
    }
    return st;
}

bool epd_is_initialized(const epd_t *dev)
{
    return (dev != NULL) && dev->initialized;
}

uint16_t epd_width(const epd_t *dev)
{
    if (dev == NULL || dev->panel == NULL) {
        return 0U;
    }
    return dev->panel->width;
}

uint16_t epd_height(const epd_t *dev)
{
    if (dev == NULL || dev->panel == NULL) {
        return 0U;
    }
    return dev->panel->height;
}

uint16_t epd_bytes_per_row(const epd_t *dev)
{
    if (dev == NULL || dev->panel == NULL) {
        return 0U;
    }
    return epd_panel_bytes_per_row(dev->panel);
}

uint32_t epd_plane_size(const epd_t *dev)
{
    if (dev == NULL || dev->panel == NULL) {
        return 0U;
    }
    return epd_panel_plane_size(dev->panel);
}
