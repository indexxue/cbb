/**
 * @file    gc9a01.c
 * @brief   GC9A01 初始化与写显存；序列对齐常见 1.28" 圆屏模组（Espressif/Waveshare 同类）。
 */

#include "gc9a01.h"

#include "gc9a01_config.h"

#include <stddef.h>
#include <stdint.h>

#define GC9A01_CMD_SWRESET 0x01U
#define GC9A01_CMD_SLPOUT 0x11U
#define GC9A01_CMD_INVON 0x21U
#define GC9A01_CMD_DISPON 0x29U
#define GC9A01_CMD_CASET 0x2AU
#define GC9A01_CMD_RASET 0x2BU
#define GC9A01_CMD_RAMWR 0x2CU
#define GC9A01_CMD_MADCTL 0x36U
#define GC9A01_CMD_COLMOD 0x3AU

#define MADCTL_MY 0x80U
#define MADCTL_MX 0x40U
#define MADCTL_MV 0x20U
#define MADCTL_BGR 0x08U

static uint8_t s_rgb565_tx_chunk[GC9A01_TX_CHUNK];
static uint16_t s_linebuf[GC9A01_LINEBUF_MAX];

static void dc_cmd(gc9a01_t *dev)
{
    if (dev->set_dc) {
        dev->set_dc(0);
    }
}

static void dc_data(gc9a01_t *dev)
{
    if (dev->set_dc) {
        dev->set_dc(1);
    }
}

static void cs_low(gc9a01_t *dev)
{
    if (dev->set_cs) {
        dev->set_cs(0);
    }
}

static void cs_high(gc9a01_t *dev)
{
    if (dev->set_cs) {
        dev->set_cs(1);
    }
}

static void delay_ms(gc9a01_t *dev, uint32_t ms)
{
    if (dev->delay_ms) {
        dev->delay_ms(ms);
    }
}

static void write_cmd(gc9a01_t *dev, uint8_t cmd)
{
    dc_cmd(dev);
    cs_low(dev);
    if (dev->spi_tx) {
        dev->spi_tx(&cmd, 1);
    }
    cs_high(dev);
}

static void write_data(gc9a01_t *dev, uint8_t data)
{
    dc_data(dev);
    cs_low(dev);
    if (dev->spi_tx) {
        dev->spi_tx(&data, 1);
    }
    cs_high(dev);
}

static void write_data_buf(gc9a01_t *dev, const uint8_t *buf, uint16_t len)
{
    if ((len == 0U) || (dev->spi_tx == NULL)) {
        return;
    }
    dc_data(dev);
    cs_low(dev);
    dev->spi_tx(buf, len);
    cs_high(dev);
}

static void write_cmd_data(gc9a01_t *dev, uint8_t cmd, const uint8_t *data, uint8_t len)
{
    write_cmd(dev, cmd);
    if ((data != NULL) && (len > 0U)) {
        write_data_buf(dev, data, len);
    }
}

static uint8_t madctl_for_rotation(uint8_t rot)
{
    switch (rot) {
    case GC9A01_ROT_90:
        return (uint8_t)(MADCTL_MX | MADCTL_MV | MADCTL_BGR);
    case GC9A01_ROT_180:
        return (uint8_t)(MADCTL_MX | MADCTL_MY | MADCTL_BGR);
    case GC9A01_ROT_270:
        return (uint8_t)(MADCTL_MY | MADCTL_MV | MADCTL_BGR);
    case GC9A01_ROT_0:
    default:
        /* 多数 1.28" 圆屏模组需 MX，否则文字左右镜像 */
        return (uint8_t)(MADCTL_MX | MADCTL_BGR);
    }
}

static void hw_reset(gc9a01_t *dev)
{
    if (dev->set_rst == NULL) {
        delay_ms(dev, 120);
        return;
    }
    dev->set_rst(1);
    delay_ms(dev, 10);
    dev->set_rst(0);
    delay_ms(dev, 20);
    dev->set_rst(1);
    delay_ms(dev, 120);
}

gc9a01_status_t gc9a01_register(gc9a01_t *dev, const gc9a01_config_t *cfg)
{
    static const uint8_t d14[] = {0x14};
    static const uint8_t d40[] = {0x40};
    static const uint8_t dff[] = {0xFF};
    static const uint8_t d0a[] = {0x0A};
    static const uint8_t d21[] = {0x21};
    static const uint8_t d00[] = {0x00};
    static const uint8_t d80[] = {0x80};
    static const uint8_t d01[] = {0x01};
    static const uint8_t db6[] = {0x00, 0x00};
    static const uint8_t d05[] = {0x05};

    if ((dev == NULL) || (cfg == NULL) || (cfg->spi_tx == NULL)) {
        return GC9A01_ERROR_PARAM;
    }

    dev->spi_tx = cfg->spi_tx;
    dev->set_cs = cfg->set_cs;
    dev->set_dc = cfg->set_dc;
    dev->set_rst = cfg->set_rst;
    dev->set_bl = cfg->set_bl;
    dev->delay_ms = cfg->delay_ms;
    dev->rotation = (cfg->rotation <= 3U) ? cfg->rotation : (uint8_t)GC9A01_ROT_0;
    dev->initialized = false;

    cs_high(dev);
    dc_data(dev);
    hw_reset(dev);

    write_cmd(dev, 0xEFU);
    write_cmd_data(dev, 0xEBU, d14, 1);
    write_cmd(dev, 0xFEU);
    write_cmd(dev, 0xEFU);
    write_cmd_data(dev, 0xEBU, d14, 1);
    write_cmd_data(dev, 0x84U, d40, 1);
    write_cmd_data(dev, 0x85U, dff, 1);
    write_cmd_data(dev, 0x86U, dff, 1);
    write_cmd_data(dev, 0x87U, dff, 1);
    write_cmd_data(dev, 0x88U, d0a, 1);
    write_cmd_data(dev, 0x89U, d21, 1);
    write_cmd_data(dev, 0x8AU, d00, 1);
    write_cmd_data(dev, 0x8BU, d80, 1);
    write_cmd_data(dev, 0x8CU, d01, 1);
    write_cmd_data(dev, 0x8DU, d01, 1);
    write_cmd_data(dev, 0x8EU, dff, 1);
    write_cmd_data(dev, 0x8FU, dff, 1);
    write_cmd_data(dev, 0xB6U, db6, 2);
    {
        uint8_t madctl = madctl_for_rotation(dev->rotation);
        write_cmd_data(dev, GC9A01_CMD_MADCTL, &madctl, 1);
    }
    write_cmd_data(dev, GC9A01_CMD_COLMOD, d05, 1);
    write_cmd_data(dev, 0x90U, (const uint8_t[]){0x08, 0x08, 0x08, 0x08}, 4);
    write_cmd_data(dev, 0xBDU, (const uint8_t[]){0x06}, 1);
    write_cmd_data(dev, 0xBCU, (const uint8_t[]){0x00}, 1);
    write_cmd_data(dev, 0xFFU, (const uint8_t[]){0x60, 0x01, 0x04}, 3);
    write_cmd_data(dev, 0xC3U, (const uint8_t[]){0x13}, 1);
    write_cmd_data(dev, 0xC4U, (const uint8_t[]){0x13}, 1);
    write_cmd_data(dev, 0xC9U, (const uint8_t[]){0x22}, 1);
    write_cmd_data(dev, 0xBEU, (const uint8_t[]){0x11}, 1);
    write_cmd_data(dev, 0xE1U, (const uint8_t[]){0x10, 0x0E}, 2);
    write_cmd_data(dev, 0xDFU, (const uint8_t[]){0x21, 0x0C, 0x02}, 3);
    write_cmd_data(dev, 0xF0U, (const uint8_t[]){0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}, 6);
    write_cmd_data(dev, 0xF1U, (const uint8_t[]){0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}, 6);
    write_cmd_data(dev, 0xF2U, (const uint8_t[]){0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}, 6);
    write_cmd_data(dev, 0xF3U, (const uint8_t[]){0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}, 6);
    write_cmd_data(dev, 0xEDU, (const uint8_t[]){0x1B, 0x0B}, 2);
    write_cmd_data(dev, 0xAEU, (const uint8_t[]){0x77}, 1);
    write_cmd_data(dev, 0xCDU, (const uint8_t[]){0x63}, 1);
    write_cmd_data(dev, 0x70U, (const uint8_t[]){0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03}, 9);
    write_cmd_data(dev, 0xE8U, (const uint8_t[]){0x34}, 1);
    write_cmd_data(dev, 0x62U,
                   (const uint8_t[]){0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70}, 12);
    write_cmd_data(dev, 0x63U,
                   (const uint8_t[]){0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70}, 12);
    write_cmd_data(dev, 0x64U, (const uint8_t[]){0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07}, 7);
    write_cmd_data(dev, 0x66U,
                   (const uint8_t[]){0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00}, 10);
    write_cmd_data(dev, 0x67U,
                   (const uint8_t[]){0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98}, 10);
    write_cmd_data(dev, 0x74U, (const uint8_t[]){0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00}, 7);
    write_cmd_data(dev, 0x98U, (const uint8_t[]){0x3E, 0x07}, 2);
    write_cmd(dev, 0x35U);
    write_cmd(dev, GC9A01_CMD_INVON);

    write_cmd(dev, GC9A01_CMD_SLPOUT);
    delay_ms(dev, 120);
    write_cmd(dev, GC9A01_CMD_DISPON);
    delay_ms(dev, 20);

    (void)gc9a01_set_backlight(dev, true);
    dev->initialized = true;
    return GC9A01_OK;
}

gc9a01_status_t gc9a01_set_backlight(gc9a01_t *dev, bool on)
{
    if (dev == NULL) {
        return GC9A01_ERROR_PARAM;
    }
    if (dev->set_bl) {
        dev->set_bl(on ? 1 : 0);
    }
    return GC9A01_OK;
}

gc9a01_status_t gc9a01_set_window(gc9a01_t *dev, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t buf[4];
    uint8_t ramwr;

    if ((dev == NULL) || !dev->initialized) {
        return GC9A01_ERROR_NOT_INIT;
    }

    write_cmd(dev, GC9A01_CMD_CASET);
    buf[0] = (uint8_t)(x0 >> 8);
    buf[1] = (uint8_t)(x0 & 0xFFU);
    buf[2] = (uint8_t)(x1 >> 8);
    buf[3] = (uint8_t)(x1 & 0xFFU);
    write_data_buf(dev, buf, 4);

    write_cmd(dev, GC9A01_CMD_RASET);
    buf[0] = (uint8_t)(y0 >> 8);
    buf[1] = (uint8_t)(y0 & 0xFFU);
    buf[2] = (uint8_t)(y1 >> 8);
    buf[3] = (uint8_t)(y1 & 0xFFU);
    write_data_buf(dev, buf, 4);

    ramwr = (uint8_t)GC9A01_CMD_RAMWR;
    dc_cmd(dev);
    cs_low(dev);
    if (dev->spi_tx) {
        dev->spi_tx(&ramwr, 1);
    }
    return GC9A01_OK;
}

gc9a01_status_t gc9a01_write_pixels(gc9a01_t *dev, const uint16_t *buf, uint32_t len)
{
    uint32_t idx;

    if ((dev == NULL) || !dev->initialized) {
        return GC9A01_ERROR_NOT_INIT;
    }
    if ((buf == NULL) || (len == 0U) || (dev->spi_tx == NULL)) {
        return GC9A01_ERROR_PARAM;
    }

    dc_data(dev);
    idx = 0;
    while (idx < len) {
        uint32_t n = len - idx;
        uint32_t max_pairs = GC9A01_TX_CHUNK / 2U;
        uint32_t k;

        if (n > max_pairs) {
            n = max_pairs;
        }
        for (k = 0; k < n; k++) {
            uint16_t v = buf[idx + k];
            s_rgb565_tx_chunk[k * 2U] = (uint8_t)(v >> 8);
            s_rgb565_tx_chunk[k * 2U + 1U] = (uint8_t)(v & 0xFFU);
        }
        dev->spi_tx(s_rgb565_tx_chunk, (uint16_t)(n * 2U));
        idx += n;
    }
    return GC9A01_OK;
}

gc9a01_status_t gc9a01_write_pixel_bytes(gc9a01_t *dev, const uint8_t *buf, uint32_t nbytes)
{
    const uint8_t *p;

    if ((dev == NULL) || !dev->initialized || (buf == NULL) || (nbytes == 0U) || (dev->spi_tx == NULL)) {
        return GC9A01_ERROR_PARAM;
    }

    p = buf;
    dc_data(dev);
    while (nbytes > 0U) {
        uint32_t chunk = nbytes;
        if (chunk > 65535U) {
            chunk = 65535U;
        }
        dev->spi_tx(p, (uint16_t)chunk);
        p += chunk;
        nbytes -= chunk;
    }
    return GC9A01_OK;
}

void gc9a01_end_write(gc9a01_t *dev)
{
    if ((dev != NULL) && (dev->set_cs != NULL)) {
        dev->set_cs(1);
    }
}

bool gc9a01_is_initialized(const gc9a01_t *dev)
{
    return (dev != NULL) && dev->initialized;
}

uint16_t gc9a01_display_width(const gc9a01_t *dev)
{
    (void)dev;
    return GC9A01_PANEL_W;
}

uint16_t gc9a01_display_height(const gc9a01_t *dev)
{
    (void)dev;
    return GC9A01_PANEL_H;
}

gc9a01_rotation_t gc9a01_get_rotation(const gc9a01_t *dev)
{
    if ((dev == NULL) || !dev->initialized) {
        return GC9A01_ROT_0;
    }
    return (gc9a01_rotation_t)dev->rotation;
}

void gc9a01_fill(gc9a01_t *dev, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    uint16_t row_w;
    uint16_t row;
    uint16_t i;

    if ((dev == NULL) || !dev->initialized || (x1 <= x0) || (y1 <= y0)) {
        return;
    }
    row_w = (uint16_t)(x1 - x0);
    if (row_w > GC9A01_LINEBUF_MAX) {
        return;
    }
    for (i = 0; i < row_w; i++) {
        s_linebuf[i] = color;
    }
    if (gc9a01_set_window(dev, x0, y0, (uint16_t)(x1 - 1U), (uint16_t)(y1 - 1U)) != GC9A01_OK) {
        return;
    }
    for (row = y0; row < y1; row++) {
        if (gc9a01_write_pixels(dev, s_linebuf, row_w) != GC9A01_OK) {
            break;
        }
    }
    gc9a01_end_write(dev);
}
