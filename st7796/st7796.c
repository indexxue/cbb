/**
 * @file    st7796.c
 * @brief   ST7796 寄存器与写显存；320x480 全幅窗口（无 GRAM 偏移）。
 *
 * @note   在 ESP-IDF SPI Master 上为设备配置 `spics_io_num = -1`，由 `set_cs` 软件片选，
 *         以便 `RAMWR` 后可多次 `spi_tx` 而 CS 保持有效。
 */

#include "st7796.h"
#include "st7796_config.h"

#include <stddef.h>
#include <stdint.h>

#define ST7796_CMD_SWRESET 0x01U
#define ST7796_CMD_SLPOUT 0x11U
#define ST7796_CMD_INVOFF 0x20U
#define ST7796_CMD_DISPON 0x29U
#define ST7796_CMD_CASET 0x2AU
#define ST7796_CMD_RASET 0x2BU
#define ST7796_CMD_RAMWR 0x2CU
#define ST7796_CMD_MADCTL 0x36U
#define ST7796_CMD_COLMOD 0x3AU
#define ST7796_CMD_DFUNCTR 0xB6U
#define ST7796_CMD_EMSET 0xB7U
#define ST7796_CMD_PWCTR1 0xC0U
#define ST7796_CMD_PWCTR2 0xC1U
#define ST7796_CMD_PWCTR3 0xC2U
#define ST7796_CMD_VMCTR1 0xC5U
#define ST7796_CMD_DOCA 0xE8U
#define ST7796_CMD_PVGAMCTRL 0xE0U
#define ST7796_CMD_NVGAMCTRL 0xE1U
#define ST7796_CMD_CSCON 0xF0U

#define MADCTL_MY 0x80U
#define MADCTL_MX 0x40U
#define MADCTL_MV 0x20U
#define MADCTL_BGR 0x08U

static uint8_t s_rgb565_tx_chunk[ST7796_TX_CHUNK];

static void dc_cmd(st7796_t *dev)
{
    if (dev->set_dc) {
        dev->set_dc(0);
    }
}

static void dc_data(st7796_t *dev)
{
    if (dev->set_dc) {
        dev->set_dc(1);
    }
}

static void cs_low(st7796_t *dev)
{
    if (dev->set_cs) {
        dev->set_cs(0);
    }
}

static void cs_high(st7796_t *dev)
{
    if (dev->set_cs) {
        dev->set_cs(1);
    }
}

static void rst_high(st7796_t *dev)
{
    if (dev->set_rst) {
        dev->set_rst(1);
    }
}

static void rst_low(st7796_t *dev)
{
    if (dev->set_rst) {
        dev->set_rst(0);
    }
}

static void delay_ms(st7796_t *dev, uint32_t ms)
{
    if (dev->delay_ms) {
        dev->delay_ms(ms);
    }
}

static void write_cmd(st7796_t *dev, uint8_t cmd)
{
    dc_cmd(dev);
    cs_low(dev);
    if (dev->spi_tx) {
        dev->spi_tx(&cmd, 1U);
    }
    cs_high(dev);
}

static void write_data(st7796_t *dev, uint8_t data)
{
    dc_data(dev);
    cs_low(dev);
    if (dev->spi_tx) {
        dev->spi_tx(&data, 1U);
    }
    cs_high(dev);
}

static void write_data_buf(st7796_t *dev, const uint8_t *buf, uint16_t len)
{
    if ((len == 0U) || (dev->spi_tx == NULL)) {
        return;
    }
    dc_data(dev);
    cs_low(dev);
    dev->spi_tx(buf, len);
    cs_high(dev);
}

static uint8_t madctl_for_rotation(uint8_t rot)
{
    switch (rot) {
    case ST7796_ROT_PORTRAIT_0:
        return (uint8_t)(MADCTL_MX);
    case ST7796_ROT_PORTRAIT_180:
        return (uint8_t)(MADCTL_MY);
    case ST7796_ROT_LANDSCAPE:
        return (uint8_t)(MADCTL_MV);
    default:
        return (uint8_t)(MADCTL_MX | MADCTL_MY | MADCTL_MV);
    }
}

static void apply_init_sequence(st7796_t *dev, uint8_t bgr)
{
    uint8_t madctl;

    write_cmd(dev, ST7796_CMD_SWRESET);
    delay_ms(dev, 150U);

    write_cmd(dev, ST7796_CMD_SLPOUT);
    delay_ms(dev, 120U);

    /* Unlock extended command set */
    write_cmd(dev, ST7796_CMD_CSCON);
    write_data(dev, 0xC3U);
    write_cmd(dev, ST7796_CMD_CSCON);
    write_data(dev, 0x96U);

    write_cmd(dev, ST7796_CMD_MADCTL);
    madctl = madctl_for_rotation(dev->rotation);
    if (bgr != 0U) {
        madctl = (uint8_t)(madctl | MADCTL_BGR);
    }
    write_data(dev, madctl);

    write_cmd(dev, ST7796_CMD_COLMOD);
    write_data(dev, 0x55U); /* 16-bit/pixel */

    write_cmd(dev, ST7796_CMD_DFUNCTR);
    write_data(dev, 0x80U);
    write_data(dev, 0x02U);
    write_data(dev, 0x3BU);

    write_cmd(dev, ST7796_CMD_DOCA);
    write_data(dev, 0x40U);
    write_data(dev, 0x8AU);
    write_data(dev, 0x00U);
    write_data(dev, 0x00U);
    write_data(dev, 0x29U);
    write_data(dev, 0x19U);
    write_data(dev, 0xA5U);
    write_data(dev, 0x33U);

    write_cmd(dev, ST7796_CMD_PWCTR1);
    write_data(dev, 0x80U);
    write_data(dev, 0x64U);

    write_cmd(dev, ST7796_CMD_PWCTR2);
    write_data(dev, 0x13U);

    write_cmd(dev, ST7796_CMD_PWCTR3);
    write_data(dev, 0xA7U);

    write_cmd(dev, ST7796_CMD_VMCTR1);
    write_data(dev, 0x27U);

    write_cmd(dev, ST7796_CMD_EMSET);
    write_data(dev, 0xC6U);

    write_cmd(dev, ST7796_CMD_PVGAMCTRL);
    write_data(dev, 0xF0U);
    write_data(dev, 0x09U);
    write_data(dev, 0x0BU);
    write_data(dev, 0x06U);
    write_data(dev, 0x04U);
    write_data(dev, 0x15U);
    write_data(dev, 0x2FU);
    write_data(dev, 0x54U);
    write_data(dev, 0x42U);
    write_data(dev, 0x3CU);
    write_data(dev, 0x17U);
    write_data(dev, 0x14U);
    write_data(dev, 0x18U);
    write_data(dev, 0x1BU);

    write_cmd(dev, ST7796_CMD_NVGAMCTRL);
    write_data(dev, 0xE0U);
    write_data(dev, 0x09U);
    write_data(dev, 0x0BU);
    write_data(dev, 0x06U);
    write_data(dev, 0x04U);
    write_data(dev, 0x03U);
    write_data(dev, 0x2BU);
    write_data(dev, 0x43U);
    write_data(dev, 0x42U);
    write_data(dev, 0x3BU);
    write_data(dev, 0x16U);
    write_data(dev, 0x14U);
    write_data(dev, 0x17U);
    write_data(dev, 0x1BU);

    /* Lock command set */
    write_cmd(dev, ST7796_CMD_CSCON);
    write_data(dev, 0x3CU);
    write_cmd(dev, ST7796_CMD_CSCON);
    write_data(dev, 0x69U);

    write_cmd(dev, ST7796_CMD_INVOFF);
    delay_ms(dev, 10U);

    write_cmd(dev, ST7796_CMD_DISPON);
    delay_ms(dev, 20U);
}

st7796_status_t st7796_register(st7796_t *dev, const st7796_config_t *cfg)
{
    if ((dev == NULL) || (cfg == NULL) || (cfg->spi_tx == NULL)) {
        return ST7796_ERROR_PARAM;
    }

    dev->spi_tx = cfg->spi_tx;
    dev->set_cs = cfg->set_cs;
    dev->set_dc = cfg->set_dc;
    dev->set_rst = cfg->set_rst;
    dev->set_bl = cfg->set_bl;
    dev->delay_ms = cfg->delay_ms;
    dev->rotation = (cfg->rotation <= 3U) ? cfg->rotation : (uint8_t)ST7796_ROT_PORTRAIT_0;
    dev->initialized = false;

    cs_high(dev);
    dc_data(dev);
    rst_high(dev);
    delay_ms(dev, 10U);
    rst_low(dev);
    delay_ms(dev, 20U);
    rst_high(dev);
    delay_ms(dev, 120U);

    apply_init_sequence(dev, cfg->bgr);

    (void)st7796_set_backlight(dev, true);
    dev->initialized = true;
    return ST7796_OK;
}

st7796_status_t st7796_set_backlight(st7796_t *dev, bool on)
{
    if (dev == NULL) {
        return ST7796_ERROR_PARAM;
    }
    if (dev->set_bl) {
        dev->set_bl(on ? 1 : 0);
    }
    return ST7796_OK;
}

st7796_status_t st7796_set_window(st7796_t *dev, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t buf[4];
    uint8_t ramwr;

    if ((dev == NULL) || !dev->initialized) {
        return ST7796_ERROR_NOT_INIT;
    }

    write_cmd(dev, ST7796_CMD_CASET);
    buf[0] = (uint8_t)(x0 >> 8);
    buf[1] = (uint8_t)(x0 & 0xFFU);
    buf[2] = (uint8_t)(x1 >> 8);
    buf[3] = (uint8_t)(x1 & 0xFFU);
    write_data_buf(dev, buf, 4U);

    write_cmd(dev, ST7796_CMD_RASET);
    buf[0] = (uint8_t)(y0 >> 8);
    buf[1] = (uint8_t)(y0 & 0xFFU);
    buf[2] = (uint8_t)(y1 >> 8);
    buf[3] = (uint8_t)(y1 & 0xFFU);
    write_data_buf(dev, buf, 4U);

    ramwr = (uint8_t)ST7796_CMD_RAMWR;
    dc_cmd(dev);
    cs_low(dev);
    if (dev->spi_tx) {
        dev->spi_tx(&ramwr, 1U);
    }
    return ST7796_OK;
}

st7796_status_t st7796_write_pixels(st7796_t *dev, const uint16_t *buf, uint32_t len)
{
    uint32_t idx;

    if ((dev == NULL) || !dev->initialized) {
        return ST7796_ERROR_NOT_INIT;
    }
    if ((buf == NULL) || (len == 0U) || (dev->spi_tx == NULL)) {
        return ST7796_ERROR_PARAM;
    }

    dc_data(dev);
    idx = 0U;
    while (idx < len) {
        uint32_t n = len - idx;
        uint32_t max_pairs = ST7796_TX_CHUNK / 2U;
        uint32_t k;

        if (n > max_pairs) {
            n = max_pairs;
        }
        for (k = 0U; k < n; k++) {
            uint16_t v = buf[idx + k];
            s_rgb565_tx_chunk[k * 2U] = (uint8_t)(v >> 8);
            s_rgb565_tx_chunk[k * 2U + 1U] = (uint8_t)(v & 0xFFU);
        }
        dev->spi_tx(s_rgb565_tx_chunk, (uint16_t)(n * 2U));
        idx += n;
    }
    return ST7796_OK;
}

st7796_status_t st7796_write_pixel_bytes(st7796_t *dev, const uint8_t *buf, uint32_t nbytes)
{
    const uint8_t *p;

    if ((dev == NULL) || !dev->initialized || (buf == NULL) || (nbytes == 0U) ||
        (dev->spi_tx == NULL)) {
        return ST7796_ERROR_PARAM;
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
    return ST7796_OK;
}

void st7796_end_write(st7796_t *dev)
{
    if ((dev != NULL) && (dev->set_cs != NULL)) {
        dev->set_cs(1);
    }
}

bool st7796_is_initialized(const st7796_t *dev)
{
    return (dev != NULL) && dev->initialized;
}

uint16_t st7796_display_width(const st7796_t *dev)
{
    if ((dev == NULL) || !dev->initialized) {
        return 0U;
    }
    if ((dev->rotation == (uint8_t)ST7796_ROT_PORTRAIT_0) ||
        (dev->rotation == (uint8_t)ST7796_ROT_PORTRAIT_180)) {
        return ST7796_PANEL_W;
    }
    return ST7796_PANEL_H;
}

uint16_t st7796_display_height(const st7796_t *dev)
{
    if ((dev == NULL) || !dev->initialized) {
        return 0U;
    }
    if ((dev->rotation == (uint8_t)ST7796_ROT_PORTRAIT_0) ||
        (dev->rotation == (uint8_t)ST7796_ROT_PORTRAIT_180)) {
        return ST7796_PANEL_H;
    }
    return ST7796_PANEL_W;
}

st7796_rotation_t st7796_get_rotation(const st7796_t *dev)
{
    if (dev == NULL) {
        return ST7796_ROT_PORTRAIT_0;
    }
    return (st7796_rotation_t)dev->rotation;
}
