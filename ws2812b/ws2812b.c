/**
 * @file    ws2812b.c
 * @brief   WS2812B pixel buffer and USER / SPI output backends (no MCU HAL).
 */

#include "ws2812b.h"

#include <stdlib.h>
#include <string.h>

void ws2812b_encode_grb_byte_spi(uint8_t grb_byte, uint8_t *dst8, uint8_t codeword0, uint8_t codeword1)
{
    uint8_t i;
    for (i = 0u; i < 8u; i++) {
        const uint8_t mask = (uint8_t)(0x80u >> i);
        dst8[i]            = (grb_byte & mask) ? codeword1 : codeword0;
    }
}

static ws2812b_status_t ws2812b_refresh_spi(ws2812b_t *dev)
{
    const size_t grb_len = (size_t)dev->num_leds * 3u;
    size_t off             = 0u;
    size_t i;

    for (i = 0u; i < grb_len; i++) {
        ws2812b_encode_grb_byte_spi(dev->pixels[i], dev->spi_buf + off, dev->spi_codeword0, dev->spi_codeword1);
        off += 8u;
    }

    return (dev->spi_write(dev->spi_buf, dev->spi_buf_len, dev->ctx) != 0) ? WS2812B_ERROR_IO : WS2812B_OK;
}

static void ws2812b_free_buffers(ws2812b_t *dev)
{
    if ((dev->pixels != NULL) && dev->pixels_owned) {
        free(dev->pixels);
    }
    dev->pixels = NULL;
    dev->pixels_owned = false;

    if ((dev->spi_buf != NULL) && dev->spi_buf_owned) {
        free(dev->spi_buf);
    }
    dev->spi_buf = NULL;
    dev->spi_buf_len = 0u;
    dev->spi_buf_owned = false;
}

ws2812b_status_t ws2812b_init_with_config(ws2812b_t *dev, const ws2812b_config_t *cfg)
{
    if (dev == NULL || cfg == NULL || cfg->num_leds == 0u) {
        return WS2812B_ERROR_PARAM;
    }

    if (cfg->bus == WS2812B_BUS_USER) {
        if (cfg->transmit == NULL) {
            return WS2812B_ERROR_PARAM;
        }
    } else if (cfg->bus == WS2812B_BUS_SPI) {
        if (cfg->spi_write == NULL) {
            return WS2812B_ERROR_PARAM;
        }
    } else {
        return WS2812B_ERROR_PARAM;
    }

    if (dev->initialized) {
        ws2812b_deinit(dev);
    }

    const size_t grb_len = (size_t)cfg->num_leds * 3u;
    uint8_t     *pix     = (uint8_t *)malloc(grb_len);
    if (pix == NULL) {
        return WS2812B_ERROR_NOMEM;
    }
    memset(pix, 0, grb_len);

    uint8_t *spi_buf     = NULL;
    size_t   spi_buf_len = 0u;
    if (cfg->bus == WS2812B_BUS_SPI) {
        spi_buf_len = grb_len * 8u;
        spi_buf     = (uint8_t *)malloc(spi_buf_len);
        if (spi_buf == NULL) {
            free(pix);
            return WS2812B_ERROR_NOMEM;
        }
    }

    dev->pixels           = pix;
    dev->pixels_owned     = true;
    dev->spi_buf          = spi_buf;
    dev->spi_buf_len      = spi_buf_len;
    dev->spi_buf_owned    = (spi_buf != NULL);
    dev->num_leds         = cfg->num_leds;
    dev->bus              = cfg->bus;
    dev->transmit         = cfg->transmit;
    dev->spi_write        = cfg->spi_write;
    dev->ctx              = cfg->ctx;
    dev->spi_codeword0    = (cfg->spi_codeword0 != 0u) ? cfg->spi_codeword0 : WS2812B_SPI_CODEWORD0_DEFAULT;
    dev->spi_codeword1    = (cfg->spi_codeword1 != 0u) ? cfg->spi_codeword1 : WS2812B_SPI_CODEWORD1_DEFAULT;
    dev->platform         = NULL;
    dev->platform_deinit  = NULL;
    dev->initialized      = true;
    return WS2812B_OK;
}

ws2812b_status_t ws2812b_init_user(ws2812b_t *dev,
                                   uint16_t num_leds,
                                   ws2812b_transmit_t transmit,
                                   void *ctx)
{
    ws2812b_config_t cfg = {
        .num_leds  = num_leds,
        .bus       = WS2812B_BUS_USER,
        .transmit  = transmit,
        .ctx       = ctx,
    };
    return ws2812b_init_with_config(dev, &cfg);
}

ws2812b_status_t ws2812b_init_user_buf(ws2812b_t *dev,
                                       uint16_t num_leds,
                                       uint8_t *pixel_buf,
                                       size_t pixel_buf_len,
                                       ws2812b_transmit_t transmit,
                                       void *ctx)
{
    const size_t need = (size_t)num_leds * 3u;

    if ((dev == NULL) || (num_leds == 0u) || (transmit == NULL) ||
        (pixel_buf == NULL) || (pixel_buf_len < need)) {
        return WS2812B_ERROR_PARAM;
    }

    if (dev->initialized) {
        ws2812b_deinit(dev);
    }

    memset(pixel_buf, 0, need);
    memset(dev, 0, sizeof(*dev));
    dev->pixels          = pixel_buf;
    dev->pixels_owned    = false;
    dev->num_leds        = num_leds;
    dev->bus             = WS2812B_BUS_USER;
    dev->transmit        = transmit;
    dev->ctx             = ctx;
    dev->spi_codeword0   = WS2812B_SPI_CODEWORD0_DEFAULT;
    dev->spi_codeword1   = WS2812B_SPI_CODEWORD1_DEFAULT;
    dev->initialized     = true;
    return WS2812B_OK;
}

ws2812b_status_t ws2812b_init_spi(ws2812b_t *dev,
                                  uint16_t num_leds,
                                  ws2812b_spi_write_t spi_write,
                                  void *ctx)
{
    ws2812b_config_t cfg = {
        .num_leds  = num_leds,
        .bus       = WS2812B_BUS_SPI,
        .spi_write = spi_write,
        .ctx       = ctx,
    };
    return ws2812b_init_with_config(dev, &cfg);
}

void ws2812b_deinit(ws2812b_t *dev)
{
    if (dev == NULL || !dev->initialized) {
        return;
    }
    if (dev->platform_deinit != NULL && dev->platform != NULL) {
        dev->platform_deinit(dev->platform);
    }
    ws2812b_free_buffers(dev);
    dev->transmit        = NULL;
    dev->spi_write       = NULL;
    dev->ctx             = NULL;
    dev->num_leds        = 0u;
    dev->platform        = NULL;
    dev->platform_deinit = NULL;
    dev->initialized     = false;
}

bool ws2812b_is_initialized(const ws2812b_t *dev)
{
    return dev != NULL && dev->initialized;
}

ws2812b_status_t ws2812b_set_pixel_rgb(ws2812b_t *dev, uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    return ws2812b_set_pixel_grb(dev, index, g, r, b);
}

ws2812b_status_t ws2812b_set_pixel_grb(ws2812b_t *dev, uint16_t index, uint8_t g, uint8_t r, uint8_t b)
{
    if (dev == NULL || !dev->initialized) {
        return WS2812B_ERROR_NOT_INIT;
    }
    if (index >= dev->num_leds) {
        return WS2812B_ERROR_PARAM;
    }
    uint8_t *p = dev->pixels + (size_t)index * 3u;
    p[0]       = g;
    p[1]       = r;
    p[2]       = b;
    return WS2812B_OK;
}

ws2812b_status_t ws2812b_refresh(ws2812b_t *dev)
{
    if (dev == NULL || !dev->initialized) {
        return WS2812B_ERROR_NOT_INIT;
    }

    const size_t len = (size_t)dev->num_leds * 3u;
    if (dev->bus == WS2812B_BUS_SPI) {
        return ws2812b_refresh_spi(dev);
    }
    return (dev->transmit(dev->pixels, len, dev->ctx) != 0) ? WS2812B_ERROR_IO : WS2812B_OK;
}

ws2812b_status_t ws2812b_clear(ws2812b_t *dev)
{
    if (dev == NULL || !dev->initialized) {
        return WS2812B_ERROR_NOT_INIT;
    }
    memset(dev->pixels, 0, (size_t)dev->num_leds * 3u);
    return ws2812b_refresh(dev);
}

uint8_t *ws2812b_get_pixels(ws2812b_t *dev)
{
    if (dev == NULL || !dev->initialized) {
        return NULL;
    }
    return dev->pixels;
}

uint16_t ws2812b_get_num_leds(const ws2812b_t *dev)
{
    if (dev == NULL) {
        return 0u;
    }
    return dev->num_leds;
}
