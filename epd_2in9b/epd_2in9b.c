/**
 * @file    epd_2in9b.c
 * @brief   2.9 寸 SSD1680 墨水屏驱动（GoodDisplay S-GDEY029Z95 官方流程）。
 */

#include "epd_2in9b.h"

#include <stddef.h>

#define EPD_2IN9B_BUSY_TIMEOUT_MS  45000U
/** GoodDisplay EPD_Update() 全刷控制字节。 */
#define EPD_2IN9B_UPDATE_FULL      0xF7U

static bool is_busy(const epd_2in9b_t *dev)
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

static void delay_ms(epd_2in9b_t *dev, uint32_t ms)
{
    if (dev->delay_ms) {
        dev->delay_ms(ms);
    }
}

static void cs_low(epd_2in9b_t *dev)
{
    if (dev->set_cs) {
        dev->set_cs(0);
    }
}

static void cs_high(epd_2in9b_t *dev)
{
    if (dev->set_cs) {
        dev->set_cs(1);
    }
}

static void dc_cmd(epd_2in9b_t *dev)
{
    if (dev->set_dc) {
        dev->set_dc(0);
    }
}

static void dc_data(epd_2in9b_t *dev)
{
    if (dev->set_dc) {
        dev->set_dc(1);
    }
}

static void rst_high(epd_2in9b_t *dev)
{
    if (dev->set_rst) {
        dev->set_rst(1);
    }
}

static void rst_low(epd_2in9b_t *dev)
{
    if (dev->set_rst) {
        dev->set_rst(0);
    }
}

static void write_cmd(epd_2in9b_t *dev, uint8_t cmd)
{
    dc_cmd(dev);
    cs_low(dev);
    if (dev->spi_tx) {
        dev->spi_tx(&cmd, 1);
    }
    cs_high(dev);
}

static void write_data(epd_2in9b_t *dev, uint8_t data)
{
    dc_data(dev);
    cs_low(dev);
    if (dev->spi_tx) {
        dev->spi_tx(&data, 1);
    }
    cs_high(dev);
}

static void write_data_buf(epd_2in9b_t *dev, const uint8_t *buf, uint16_t len)
{
    if (len == 0U || !dev->spi_tx) {
        return;
    }

    dc_data(dev);
    cs_low(dev);
    dev->spi_tx(buf, len);
    cs_high(dev);
}

static void hw_reset(epd_2in9b_t *dev)
{
    /* GoodDisplay S-GDEY029Z95：RST 拉低 ≥10ms 后释放，再等 ≥10ms */
    rst_low(dev);
    delay_ms(dev, 10U);
    rst_high(dev);
    delay_ms(dev, 10U);
}

static void set_windows(epd_2in9b_t *dev)
{
    const uint16_t y_end = (uint16_t)(EPD_2IN9B_HEIGHT - 1U);

    write_cmd(dev, 0x44);
    write_data(dev, 0x00);
    write_data(dev, (uint8_t)(EPD_2IN9B_BYTES_PER_ROW - 1U));
    write_cmd(dev, 0x45);
    write_data(dev, (uint8_t)(y_end & 0xFFU));
    write_data(dev, (uint8_t)((y_end >> 8) & 0xFFU));
    write_data(dev, 0x00);
    write_data(dev, 0x00);
}

static void set_cursor(epd_2in9b_t *dev)
{
    const uint16_t y_end = (uint16_t)(EPD_2IN9B_HEIGHT - 1U);

    write_cmd(dev, 0x4E);
    write_data(dev, 0x00);
    write_cmd(dev, 0x4F);
    write_data(dev, (uint8_t)(y_end & 0xFFU));
    write_data(dev, (uint8_t)((y_end >> 8) & 0xFFU));
}

/** 每次写 0x24/0x26 前重置窗口与地址计数（clear 后再 display 必须）。 */
static void prepare_ram_write(epd_2in9b_t *dev)
{
    set_windows(dev);
    set_cursor(dev);
}

static epd_2in9b_status_t turn_on_display(epd_2in9b_t *dev, uint8_t ctrl)
{
    write_cmd(dev, 0x22);
    write_data(dev, ctrl);
    write_cmd(dev, 0x20);
    return epd_2in9b_wait_idle(dev);
}

static void write_plane(epd_2in9b_t *dev, uint8_t cmd, const uint8_t *plane, bool invert)
{
    uint8_t chunk[64];
    uint32_t remain = EPD_2IN9B_PLANE_SIZE;
    uint32_t offset = 0U;

    write_cmd(dev, cmd);
    while (remain > 0U) {
        uint16_t n = (remain > (uint32_t)sizeof(chunk)) ? (uint16_t)sizeof(chunk) : (uint16_t)remain;
        uint16_t i;

        if (!invert) {
            write_data_buf(dev, &plane[offset], n);
        } else {
            for (i = 0; i < n; i++) {
                chunk[i] = (uint8_t)~plane[offset + i];
            }
            write_data_buf(dev, chunk, n);
        }
        offset += n;
        remain -= n;
    }
}

static epd_2in9b_status_t check_dev(const epd_2in9b_t *dev, bool require_init)
{
    if (dev == NULL) {
        return EPD_2IN9B_ERROR_PARAM;
    }
    if (dev->spi_tx == NULL || dev->set_dc == NULL || dev->delay_ms == NULL) {
        return EPD_2IN9B_ERROR_PARAM;
    }
    if (require_init && !dev->initialized) {
        return EPD_2IN9B_ERROR_NOT_INIT;
    }
    return EPD_2IN9B_OK;
}

epd_2in9b_status_t epd_2in9b_register(epd_2in9b_t *dev, const epd_2in9b_config_t *cfg)
{
    if (dev == NULL || cfg == NULL) {
        return EPD_2IN9B_ERROR_PARAM;
    }
    if (cfg->spi_tx == NULL || cfg->set_dc == NULL || cfg->delay_ms == NULL) {
        return EPD_2IN9B_ERROR_PARAM;
    }

    dev->spi_tx = cfg->spi_tx;
    dev->set_cs = cfg->set_cs;
    dev->set_dc = cfg->set_dc;
    dev->set_rst = cfg->set_rst;
    dev->read_busy = cfg->read_busy;
    dev->delay_ms = cfg->delay_ms;
    dev->busy_active_high = cfg->busy_active_high;
    dev->bw_invert = cfg->bw_invert;
    dev->initialized = false;

    return EPD_2IN9B_OK;
}

epd_2in9b_status_t epd_2in9b_wait_idle(epd_2in9b_t *dev)
{
    epd_2in9b_status_t st = check_dev(dev, false);
    uint32_t waited = 0U;

    if (st != EPD_2IN9B_OK) {
        return st;
    }
    if (!dev->read_busy) {
        return EPD_2IN9B_OK;
    }

    delay_ms(dev, 10U);
    waited += 10U;
    while (is_busy(dev)) {
        delay_ms(dev, 50U);
        waited += 50U;
        if (waited >= EPD_2IN9B_BUSY_TIMEOUT_MS) {
            return EPD_2IN9B_ERROR_BUSY;
        }
    }
    delay_ms(dev, 200U);
    return EPD_2IN9B_OK;
}

epd_2in9b_status_t epd_2in9b_init(epd_2in9b_t *dev)
{
    epd_2in9b_status_t st = check_dev(dev, false);

    if (st != EPD_2IN9B_OK) {
        return st;
    }

    hw_reset(dev);
    st = epd_2in9b_wait_idle(dev);
    if (st != EPD_2IN9B_OK) {
        return st;
    }

    write_cmd(dev, 0x12);
    st = epd_2in9b_wait_idle(dev);
    if (st != EPD_2IN9B_OK) {
        return st;
    }

    write_cmd(dev, 0x01);
    write_data(dev, (uint8_t)((EPD_2IN9B_HEIGHT - 1U) & 0xFFU));
    write_data(dev, (uint8_t)(((EPD_2IN9B_HEIGHT - 1U) >> 8) & 0xFFU));
    write_data(dev, 0x00);

    write_cmd(dev, 0x11);
    write_data(dev, 0x01);

    set_windows(dev);

    write_cmd(dev, 0x3C);
    write_data(dev, 0x05);

    write_cmd(dev, 0x21);
    write_data(dev, 0x00);
    write_data(dev, 0x80);

    write_cmd(dev, 0x18);
    write_data(dev, 0x80);

    set_cursor(dev);
    st = epd_2in9b_wait_idle(dev);
    if (st != EPD_2IN9B_OK) {
        return st;
    }

    dev->initialized = true;
    return EPD_2IN9B_OK;
}

epd_2in9b_status_t epd_2in9b_clear(epd_2in9b_t *dev)
{
    epd_2in9b_status_t st = check_dev(dev, true);
    uint8_t white[64];
    uint32_t remain = EPD_2IN9B_PLANE_SIZE;
    uint16_t i;

    if (st != EPD_2IN9B_OK) {
        return st;
    }

    prepare_ram_write(dev);

    for (i = 0; i < (uint16_t)sizeof(white); i++) {
        white[i] = dev->bw_invert ? 0x00U : 0xFFU;
    }

    write_cmd(dev, 0x24);
    remain = EPD_2IN9B_PLANE_SIZE;
    while (remain > 0U) {
        uint16_t n = (remain > (uint32_t)sizeof(white)) ? (uint16_t)sizeof(white) : (uint16_t)remain;
        write_data_buf(dev, white, n);
        remain -= n;
    }

    for (i = 0; i < (uint16_t)sizeof(white); i++) {
        white[i] = 0xFFU;
    }

    write_cmd(dev, 0x26);
    remain = EPD_2IN9B_PLANE_SIZE;
    while (remain > 0U) {
        uint16_t n = (remain > (uint32_t)sizeof(white)) ? (uint16_t)sizeof(white) : (uint16_t)remain;
        write_data_buf(dev, white, n);
        remain -= n;
    }

    return turn_on_display(dev, EPD_2IN9B_UPDATE_FULL);
}

epd_2in9b_status_t epd_2in9b_display_mono(epd_2in9b_t *dev, const uint8_t *bw_plane)
{
    epd_2in9b_status_t st = check_dev(dev, true);

    if (st != EPD_2IN9B_OK) {
        return st;
    }
    if (bw_plane == NULL) {
        return EPD_2IN9B_ERROR_PARAM;
    }

    prepare_ram_write(dev);
    write_plane(dev, 0x24, bw_plane, dev->bw_invert);
    return turn_on_display(dev, EPD_2IN9B_UPDATE_FULL);
}

epd_2in9b_status_t epd_2in9b_display(epd_2in9b_t *dev, const uint8_t *bw_plane, const uint8_t *red_plane)
{
    epd_2in9b_status_t st = check_dev(dev, true);

    if (st != EPD_2IN9B_OK) {
        return st;
    }
    if (bw_plane == NULL || red_plane == NULL) {
        return EPD_2IN9B_ERROR_PARAM;
    }

    prepare_ram_write(dev);

    write_plane(dev, 0x24, bw_plane, dev->bw_invert);
    write_plane(dev, 0x26, red_plane, true);
    return turn_on_display(dev, EPD_2IN9B_UPDATE_FULL);
}

epd_2in9b_status_t epd_2in9b_sleep(epd_2in9b_t *dev)
{
    epd_2in9b_status_t st = check_dev(dev, true);

    if (st != EPD_2IN9B_OK) {
        return st;
    }

    write_cmd(dev, 0x10);
    write_data(dev, 0x01);
    delay_ms(dev, 100U);
    dev->initialized = false;
    return EPD_2IN9B_OK;
}

bool epd_2in9b_is_initialized(const epd_2in9b_t *dev)
{
    return (dev != NULL) && dev->initialized;
}
