/**
 * @file    epd_2in9.c
 * @brief   2.9 寸电子墨水屏驱动实现（协议参考 Waveshare EPD_2in9 / EPD_2in9_V2）。
 */

#include "epd_2in9.h"

#include <stddef.h>

#define EPD_2IN9_LUT_V2_SIZE      159U
#define EPD_2IN9_BUSY_TIMEOUT_MS  15000U

/* V1 LUT（30 字节） */
static const uint8_t s_lut_v1_full[30] = {
    0x50, 0xAA, 0x55, 0xAA, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x1F, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t s_lut_v1_partial[30] = {
    0x10, 0x18, 0x18, 0x08, 0x18, 0x18, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x14, 0x44, 0x12, 0x00, 0x00, 0x00,
};

/* V2 局部刷新 LUT */
static const uint8_t s_lut_v2_partial[EPD_2IN9_LUT_V2_SIZE] = {
    0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22,
    0x22, 0x22, 0x00, 0x00, 0x00, 0x22, 0x17, 0x41, 0xB0, 0x32, 0x36,
};

static const uint8_t s_lut_v2_ws_20_30[EPD_2IN9_LUT_V2_SIZE] = {
    0x80, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x10, 0x66, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x80, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x00, 0x00, 0x00, 0x10, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x08, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x0A, 0x0A, 0x00, 0x0A, 0x0A, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x14, 0x08, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x44, 0x44, 0x44,
    0x44, 0x44, 0x00, 0x00, 0x00, 0x22, 0x17, 0x41, 0x00, 0x32, 0x36,
};

static const uint8_t s_lut_v2_wf_full[EPD_2IN9_LUT_V2_SIZE] = {
    0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x19, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x42, 0x22, 0x22,
    0x23, 0x32, 0x00, 0x00, 0x00, 0x22, 0x17, 0x41, 0xAE, 0x32, 0x38,
};

static bool is_busy(const epd_2in9_t *dev)
{
    if (!dev->read_busy) {
        return false;
    }

    int level = dev->read_busy();
    if (dev->busy_active_high) {
        return level != 0;
    }
    return level == 0;
}

static void delay_ms(epd_2in9_t *dev, uint32_t ms)
{
    if (dev->delay_ms) {
        dev->delay_ms(ms);
    }
}

static void cs_low(epd_2in9_t *dev)
{
    if (dev->set_cs) {
        dev->set_cs(0);
    }
}

static void cs_high(epd_2in9_t *dev)
{
    if (dev->set_cs) {
        dev->set_cs(1);
    }
}

static void dc_cmd(epd_2in9_t *dev)
{
    if (dev->set_dc) {
        dev->set_dc(0);
    }
}

static void dc_data(epd_2in9_t *dev)
{
    if (dev->set_dc) {
        dev->set_dc(1);
    }
}

static void rst_high(epd_2in9_t *dev)
{
    if (dev->set_rst) {
        dev->set_rst(1);
    }
}

static void rst_low(epd_2in9_t *dev)
{
    if (dev->set_rst) {
        dev->set_rst(0);
    }
}

static void write_cmd(epd_2in9_t *dev, uint8_t cmd)
{
    dc_cmd(dev);
    cs_low(dev);
    if (dev->spi_tx) {
        dev->spi_tx(&cmd, 1);
    }
    cs_high(dev);
}

static void write_data(epd_2in9_t *dev, uint8_t data)
{
    dc_data(dev);
    cs_low(dev);
    if (dev->spi_tx) {
        dev->spi_tx(&data, 1);
    }
    cs_high(dev);
}

static void write_data_buf(epd_2in9_t *dev, const uint8_t *buf, uint16_t len)
{
    if (len == 0U || !dev->spi_tx) {
        return;
    }

    dc_data(dev);
    cs_low(dev);
    dev->spi_tx(buf, len);
    cs_high(dev);
}

static void write_frame_fill(epd_2in9_t *dev, uint8_t cmd, uint8_t fill)
{
    uint8_t chunk[64];
    uint32_t remain = EPD_2IN9_FRAME_SIZE;

    for (uint16_t i = 0; i < (uint16_t)sizeof(chunk); i++) {
        chunk[i] = fill;
    }

    write_cmd(dev, cmd);
    while (remain > 0U) {
        uint16_t n = (remain > (uint32_t)sizeof(chunk)) ? (uint16_t)sizeof(chunk) : (uint16_t)remain;
        write_data_buf(dev, chunk, n);
        remain -= n;
    }
}

static void write_frame(epd_2in9_t *dev, uint8_t cmd, const uint8_t *image)
{
    write_cmd(dev, cmd);
    write_data_buf(dev, image, (uint16_t)EPD_2IN9_FRAME_SIZE);
}

static void hw_reset(epd_2in9_t *dev, uint32_t hold_ms, uint32_t settle_ms)
{
    rst_high(dev);
    delay_ms(dev, settle_ms);
    rst_low(dev);
    delay_ms(dev, hold_ms);
    rst_high(dev);
    delay_ms(dev, settle_ms);
}

static void v2_set_windows(epd_2in9_t *dev, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    write_cmd(dev, 0x44);
    write_data(dev, (uint8_t)((x0 >> 3) & 0xFFU));
    write_data(dev, (uint8_t)((x1 >> 3) & 0xFFU));
    write_cmd(dev, 0x45);
    write_data(dev, (uint8_t)(y0 & 0xFFU));
    write_data(dev, (uint8_t)((y0 >> 8) & 0xFFU));
    write_data(dev, (uint8_t)(y1 & 0xFFU));
    write_data(dev, (uint8_t)((y1 >> 8) & 0xFFU));
}

static void v2_set_cursor(epd_2in9_t *dev, uint16_t x, uint16_t y)
{
    write_cmd(dev, 0x4E);
    write_data(dev, (uint8_t)(x & 0xFFU));
    write_cmd(dev, 0x4F);
    write_data(dev, (uint8_t)(y & 0xFFU));
    write_data(dev, (uint8_t)((y >> 8) & 0xFFU));
}

static epd_2in9_status_t v2_lut(epd_2in9_t *dev, const uint8_t *lut)
{
    epd_2in9_status_t st;

    write_cmd(dev, 0x32);
    for (uint16_t i = 0; i < 153U; i++) {
        write_data(dev, lut[i]);
    }
    st = epd_2in9_wait_idle(dev);
    if (st != EPD_2IN9_OK) {
        return st;
    }
    return EPD_2IN9_OK;
}

static epd_2in9_status_t v2_lut_by_host(epd_2in9_t *dev, const uint8_t *lut)
{
    epd_2in9_status_t st;

    st = v2_lut(dev, lut);
    if (st != EPD_2IN9_OK) {
        return st;
    }
    write_cmd(dev, 0x3F);
    write_data(dev, lut[153]);
    write_cmd(dev, 0x03);
    write_data(dev, lut[154]);
    write_cmd(dev, 0x04);
    write_data(dev, lut[155]);
    write_data(dev, lut[156]);
    write_data(dev, lut[157]);
    write_cmd(dev, 0x2C);
    write_data(dev, lut[158]);
    return EPD_2IN9_OK;
}

static epd_2in9_status_t v2_turn_on_display(epd_2in9_t *dev, uint8_t ctrl)
{
    write_cmd(dev, 0x22);
    write_data(dev, ctrl);
    write_cmd(dev, 0x20);
    return epd_2in9_wait_idle(dev);
}

static void v1_set_windows(epd_2in9_t *dev, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    write_cmd(dev, 0x44);
    write_data(dev, (uint8_t)((x0 >> 3) & 0xFFU));
    write_data(dev, (uint8_t)((x1 >> 3) & 0xFFU));
    write_cmd(dev, 0x45);
    write_data(dev, (uint8_t)(y0 & 0xFFU));
    write_data(dev, (uint8_t)((y0 >> 8) & 0xFFU));
    write_data(dev, (uint8_t)(y1 & 0xFFU));
    write_data(dev, (uint8_t)((y1 >> 8) & 0xFFU));
}

static void v1_set_cursor(epd_2in9_t *dev, uint16_t x, uint16_t y)
{
    write_cmd(dev, 0x4E);
    write_data(dev, (uint8_t)((x >> 3) & 0xFFU));
    write_cmd(dev, 0x4F);
    write_data(dev, (uint8_t)(y & 0xFFU));
    write_data(dev, (uint8_t)((y >> 8) & 0xFFU));
}

static void v1_turn_on_display(epd_2in9_t *dev)
{
    write_cmd(dev, 0x22);
    write_data(dev, 0xC4);
    write_cmd(dev, 0x20);
    write_cmd(dev, 0xFF);
    epd_2in9_wait_idle(dev);
}

static epd_2in9_status_t check_dev(const epd_2in9_t *dev, bool require_init)
{
    if (dev == NULL) {
        return EPD_2IN9_ERROR_PARAM;
    }
    if (dev->spi_tx == NULL || dev->set_dc == NULL || dev->delay_ms == NULL) {
        return EPD_2IN9_ERROR_PARAM;
    }
    if (require_init && !dev->initialized) {
        return EPD_2IN9_ERROR_NOT_INIT;
    }
    return EPD_2IN9_OK;
}

static epd_2in9_status_t init_v1(epd_2in9_t *dev)
{
    const uint8_t *lut = (dev->lut_mode == EPD_2IN9_LUT_PARTIAL) ? s_lut_v1_partial : s_lut_v1_full;

    hw_reset(dev, 2U, 200U);

    write_cmd(dev, 0x01);
    write_data(dev, (uint8_t)((EPD_2IN9_HEIGHT - 1U) & 0xFFU));
    write_data(dev, (uint8_t)(((EPD_2IN9_HEIGHT - 1U) >> 8) & 0xFFU));
    write_data(dev, 0x00);

    write_cmd(dev, 0x0C);
    write_data(dev, 0xD7);
    write_data(dev, 0xD6);
    write_data(dev, 0x9D);

    write_cmd(dev, 0x2C);
    write_data(dev, 0xA8);

    write_cmd(dev, 0x3A);
    write_data(dev, 0x1A);

    write_cmd(dev, 0x3B);
    write_data(dev, 0x08);

    write_cmd(dev, 0x3C);
    write_data(dev, 0x03);

    write_cmd(dev, 0x11);
    write_data(dev, 0x03);

    write_cmd(dev, 0x32);
    for (uint16_t i = 0; i < 30U; i++) {
        write_data(dev, lut[i]);
    }

    dev->initialized = true;
    return EPD_2IN9_OK;
}

static epd_2in9_status_t init_v2_common(epd_2in9_t *dev, bool fast)
{
    epd_2in9_status_t st;

    hw_reset(dev, 2U, 100U);
    st = epd_2in9_wait_idle(dev);
    if (st != EPD_2IN9_OK) {
        return st;
    }

    write_cmd(dev, 0x12);
    st = epd_2in9_wait_idle(dev);
    if (st != EPD_2IN9_OK) {
        return st;
    }

    write_cmd(dev, 0x01);
    write_data(dev, 0x27);
    write_data(dev, 0x01);
    write_data(dev, 0x00);

    write_cmd(dev, 0x11);
    write_data(dev, 0x03);

    v2_set_windows(dev, 0U, 0U, (uint16_t)(EPD_2IN9_WIDTH - 1U), (uint16_t)(EPD_2IN9_HEIGHT - 1U));

    if (fast) {
        write_cmd(dev, 0x3C);
        write_data(dev, 0x05);
    }

    write_cmd(dev, 0x21);
    write_data(dev, 0x00);
    write_data(dev, 0x80);

    v2_set_cursor(dev, 0U, 0U);
    st = epd_2in9_wait_idle(dev);
    if (st != EPD_2IN9_OK) {
        return st;
    }

    st = v2_lut_by_host(dev, fast ? s_lut_v2_wf_full : s_lut_v2_ws_20_30);
    if (st != EPD_2IN9_OK) {
        return st;
    }

    dev->initialized = true;
    return EPD_2IN9_OK;
}

epd_2in9_status_t epd_2in9_register(epd_2in9_t *dev, const epd_2in9_config_t *cfg)
{
    if (dev == NULL || cfg == NULL) {
        return EPD_2IN9_ERROR_PARAM;
    }
    if (cfg->spi_tx == NULL || cfg->set_dc == NULL || cfg->delay_ms == NULL) {
        return EPD_2IN9_ERROR_PARAM;
    }

    dev->spi_tx = cfg->spi_tx;
    dev->set_cs = cfg->set_cs;
    dev->set_dc = cfg->set_dc;
    dev->set_rst = cfg->set_rst;
    dev->read_busy = cfg->read_busy;
    dev->delay_ms = cfg->delay_ms;
    dev->variant = cfg->variant;
    dev->busy_active_high = cfg->busy_active_high;
    dev->lut_mode = cfg->lut_mode;
    dev->initialized = false;

    return EPD_2IN9_OK;
}

epd_2in9_status_t epd_2in9_init(epd_2in9_t *dev)
{
    epd_2in9_status_t st = check_dev(dev, false);
    if (st != EPD_2IN9_OK) {
        return st;
    }

    if (dev->variant == EPD_2IN9_VARIANT_V1) {
        return init_v1(dev);
    }
    if (dev->variant == EPD_2IN9_VARIANT_V2) {
        return init_v2_common(dev, false);
    }
    return EPD_2IN9_ERROR_VARIANT;
}

epd_2in9_status_t epd_2in9_init_fast(epd_2in9_t *dev)
{
    epd_2in9_status_t st = check_dev(dev, false);
    if (st != EPD_2IN9_OK) {
        return st;
    }
    if (dev->variant != EPD_2IN9_VARIANT_V2) {
        return EPD_2IN9_ERROR_VARIANT;
    }
    return init_v2_common(dev, true);
}

epd_2in9_status_t epd_2in9_wait_idle(epd_2in9_t *dev)
{
    epd_2in9_status_t st = check_dev(dev, false);
    if (st != EPD_2IN9_OK) {
        return st;
    }

    if (!dev->read_busy) {
        return EPD_2IN9_OK;
    }

    uint32_t waited = 0U;

    delay_ms(dev, 10U);
    waited += 10U;
    while (is_busy(dev)) {
        delay_ms(dev, 10U);
        waited += 10U;
        if (waited >= EPD_2IN9_BUSY_TIMEOUT_MS) {
            return EPD_2IN9_ERROR_BUSY;
        }
    }
    delay_ms(dev, 10U);
    return EPD_2IN9_OK;
}

epd_2in9_status_t epd_2in9_clear(epd_2in9_t *dev)
{
    epd_2in9_status_t st = check_dev(dev, true);
    if (st != EPD_2IN9_OK) {
        return st;
    }

    if (dev->variant == EPD_2IN9_VARIANT_V2) {
        write_frame_fill(dev, 0x24, 0xFF);
        write_frame_fill(dev, 0x26, 0xFF);
        return v2_turn_on_display(dev, 0xC7);
    }

    v1_set_windows(dev, 0U, 0U, EPD_2IN9_WIDTH, EPD_2IN9_HEIGHT);
    for (uint16_t y = 0; y < EPD_2IN9_HEIGHT; y++) {
        v1_set_cursor(dev, 0U, y);
        write_cmd(dev, 0x24);
        for (uint16_t x = 0; x < EPD_2IN9_BYTES_PER_ROW; x++) {
            write_data(dev, 0xFF);
        }
    }
    v1_turn_on_display(dev);
    return EPD_2IN9_OK;
}

epd_2in9_status_t epd_2in9_display_stream(epd_2in9_t *dev, epd_2in9_fill_row_t fill_row, void *ctx)
{
    epd_2in9_status_t st = check_dev(dev, true);
    uint8_t row[EPD_2IN9_BYTES_PER_ROW];

    if (st != EPD_2IN9_OK) {
        return st;
    }
    if (fill_row == NULL) {
        return EPD_2IN9_ERROR_PARAM;
    }

    if (dev->variant == EPD_2IN9_VARIANT_V2) {
        for (uint8_t ram_cmd = 0x24U; ram_cmd <= 0x26U; ram_cmd += 2U) {
            write_cmd(dev, ram_cmd);
            for (uint16_t y = 0; y < EPD_2IN9_HEIGHT; y++) {
                fill_row(y, row, ctx);
                write_data_buf(dev, row, EPD_2IN9_BYTES_PER_ROW);
            }
        }
        return v2_turn_on_display(dev, 0xC7);
    }

    v1_set_windows(dev, 0U, 0U, EPD_2IN9_WIDTH, EPD_2IN9_HEIGHT);
    for (uint16_t y = 0; y < EPD_2IN9_HEIGHT; y++) {
        v1_set_cursor(dev, 0U, y);
        write_cmd(dev, 0x24);
        fill_row(y, row, ctx);
        write_data_buf(dev, row, EPD_2IN9_BYTES_PER_ROW);
    }
    v1_turn_on_display(dev);
    return EPD_2IN9_OK;
}

epd_2in9_status_t epd_2in9_display(epd_2in9_t *dev, const uint8_t *image)
{
    epd_2in9_status_t st = check_dev(dev, true);
    if (st != EPD_2IN9_OK) {
        return st;
    }
    if (image == NULL) {
        return EPD_2IN9_ERROR_PARAM;
    }

    if (dev->variant == EPD_2IN9_VARIANT_V2) {
        write_frame(dev, 0x24, image);
        return v2_turn_on_display(dev, 0xC7);
    }

    v1_set_windows(dev, 0U, 0U, EPD_2IN9_WIDTH, EPD_2IN9_HEIGHT);
    for (uint16_t y = 0; y < EPD_2IN9_HEIGHT; y++) {
        v1_set_cursor(dev, 0U, y);
        write_cmd(dev, 0x24);
        write_data_buf(dev, &image[(uint32_t)y * EPD_2IN9_BYTES_PER_ROW], EPD_2IN9_BYTES_PER_ROW);
    }
    v1_turn_on_display(dev);
    return EPD_2IN9_OK;
}

epd_2in9_status_t epd_2in9_display_base(epd_2in9_t *dev, const uint8_t *image)
{
    epd_2in9_status_t st = check_dev(dev, true);
    if (st != EPD_2IN9_OK) {
        return st;
    }
    if (image == NULL) {
        return EPD_2IN9_ERROR_PARAM;
    }

    if (dev->variant == EPD_2IN9_VARIANT_V2) {
        write_frame(dev, 0x24, image);
        write_frame(dev, 0x26, image);
        return v2_turn_on_display(dev, 0xC7);
    }

    return epd_2in9_display(dev, image);
}

epd_2in9_status_t epd_2in9_display_partial(epd_2in9_t *dev, const uint8_t *image)
{
    epd_2in9_status_t st = check_dev(dev, true);
    if (st != EPD_2IN9_OK) {
        return st;
    }
    if (image == NULL) {
        return EPD_2IN9_ERROR_PARAM;
    }
    if (dev->variant != EPD_2IN9_VARIANT_V2) {
        return EPD_2IN9_ERROR_VARIANT;
    }

    rst_low(dev);
    delay_ms(dev, 1U);
    rst_high(dev);
    delay_ms(dev, 2U);

    st = v2_lut(dev, s_lut_v2_partial);
    if (st != EPD_2IN9_OK) {
        return st;
    }
    write_cmd(dev, 0x37);
    for (uint8_t i = 0; i < 10U; i++) {
        uint8_t v = 0x00;
        if (i == 5U) {
            v = 0x40;
        }
        write_data(dev, v);
    }

    write_cmd(dev, 0x3C);
    write_data(dev, 0x80);

    write_cmd(dev, 0x22);
    write_data(dev, 0xC0);
    write_cmd(dev, 0x20);
    epd_2in9_wait_idle(dev);

    v2_set_windows(dev, 0U, 0U, (uint16_t)(EPD_2IN9_WIDTH - 1U), (uint16_t)(EPD_2IN9_HEIGHT - 1U));
    v2_set_cursor(dev, 0U, 0U);

    write_frame(dev, 0x24, image);
    return v2_turn_on_display(dev, 0x0F);
}

epd_2in9_status_t epd_2in9_sleep(epd_2in9_t *dev)
{
    epd_2in9_status_t st = check_dev(dev, true);
    if (st != EPD_2IN9_OK) {
        return st;
    }

    write_cmd(dev, 0x10);
    write_data(dev, 0x01);
    delay_ms(dev, 100U);
    dev->initialized = false;
    return EPD_2IN9_OK;
}

bool epd_2in9_is_initialized(const epd_2in9_t *dev)
{
    return (dev != NULL) && dev->initialized;
}

epd_2in9_variant_t epd_2in9_get_variant(const epd_2in9_t *dev)
{
    if (dev == NULL) {
        return EPD_2IN9_VARIANT_V2;
    }
    return dev->variant;
}
