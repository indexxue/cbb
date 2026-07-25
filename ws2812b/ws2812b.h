/**
 * @file    ws2812b.h
 * @brief   WS2812B RGB LED strip — MCU-agnostic pixel buffer + pluggable output backend.
 *
 * Pixel order on the wire is **GRB**. Use `ws2812b_set_pixel_rgb` for RGB semantics.
 *
 * Backends:
 * - **USER**: `transmit` callback (GPIO bit-bang, DMA, PIO, etc.) receives raw GRB bytes.
 * - **SPI**: driver expands each GRB bit to one SPI byte (8 bytes per GRB byte @ ~2.4–3.2 MHz).
 *
 * ESP-IDF RMT: see `ws2812b_esp32.h` / `ws2812b_esp32.c`.
 */

#ifndef WS2812B_H
#define WS2812B_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Default SPI codewords (1 WS2812 bit → 1 SPI byte @ ~2.4–3.2 MHz). Override via config. */
#define WS2812B_SPI_CODEWORD0_DEFAULT 0xC0u
#define WS2812B_SPI_CODEWORD1_DEFAULT 0xFCu

typedef enum {
    WS2812B_OK = 0,
    WS2812B_ERROR_PARAM,
    WS2812B_ERROR_NOT_INIT,
    WS2812B_ERROR_IO,
    WS2812B_ERROR_NOMEM
} ws2812b_status_t;

typedef enum {
    /** Board implements timing (one-wire / RMT / timer DMA …). */
    WS2812B_BUS_USER = 0,
    /** Expand GRB → SPI stream and call `spi_write`. */
    WS2812B_BUS_SPI  = 1
} ws2812b_bus_t;

/**
 * Send one GRB frame (length = 3 * num_leds). Return 0 on success.
 * Used for USER backend (includes ESP RMT after `ws2812b_esp32_init`).
 */
typedef int (*ws2812b_transmit_t)(const uint8_t *grb, size_t len, void *ctx);

/** Send expanded SPI buffer. Return 0 on success. */
typedef int (*ws2812b_spi_write_t)(const uint8_t *buf, size_t len, void *ctx);

typedef void (*ws2812b_platform_deinit_t)(void *platform);

typedef struct {
    uint16_t              num_leds;
    ws2812b_bus_t         bus;
    ws2812b_transmit_t    transmit;
    ws2812b_spi_write_t   spi_write;
    void                 *ctx;
    uint8_t               spi_codeword0;
    uint8_t               spi_codeword1;
} ws2812b_config_t;

typedef struct {
    uint8_t                  *pixels;
    uint8_t                  *spi_buf;
    size_t                    spi_buf_len;
    uint16_t                  num_leds;
    ws2812b_bus_t             bus;
    ws2812b_transmit_t        transmit;
    ws2812b_spi_write_t       spi_write;
    void                     *ctx;
    uint8_t                   spi_codeword0;
    uint8_t                   spi_codeword1;
    void                     *platform;
    ws2812b_platform_deinit_t platform_deinit;
    bool                      initialized;
    bool                      pixels_owned; /**< true: deinit 时 free(pixels) */
    bool                      spi_buf_owned;
} ws2812b_t;

ws2812b_status_t ws2812b_init_with_config(ws2812b_t *dev, const ws2812b_config_t *cfg);

/** USER backend: only `transmit` required. */
ws2812b_status_t ws2812b_init_user(ws2812b_t *dev,
                                   uint16_t num_leds,
                                   ws2812b_transmit_t transmit,
                                   void *ctx);

/**
 * USER backend with caller-owned pixel buffer (no malloc).
 * @param pixel_buf 至少 num_leds*3 字节，生命周期须覆盖整个使用期
 */
ws2812b_status_t ws2812b_init_user_buf(ws2812b_t *dev,
                                       uint16_t num_leds,
                                       uint8_t *pixel_buf,
                                       size_t pixel_buf_len,
                                       ws2812b_transmit_t transmit,
                                       void *ctx);

/** SPI backend: 8 SPI bytes per GRB byte; set SPI clock per README. */
ws2812b_status_t ws2812b_init_spi(ws2812b_t *dev,
                                  uint16_t num_leds,
                                  ws2812b_spi_write_t spi_write,
                                  void *ctx);

void ws2812b_deinit(ws2812b_t *dev);

bool ws2812b_is_initialized(const ws2812b_t *dev);

ws2812b_status_t ws2812b_set_pixel_rgb(ws2812b_t *dev, uint16_t index, uint8_t r, uint8_t g, uint8_t b);
ws2812b_status_t ws2812b_set_pixel_grb(ws2812b_t *dev, uint16_t index, uint8_t g, uint8_t r, uint8_t b);
ws2812b_status_t ws2812b_refresh(ws2812b_t *dev);
ws2812b_status_t ws2812b_clear(ws2812b_t *dev);

uint8_t *ws2812b_get_pixels(ws2812b_t *dev);
uint16_t ws2812b_get_num_leds(const ws2812b_t *dev);

/** Encode one GRB byte → 8 SPI bytes (MSB first). Used by SPI backend and tests. */
void ws2812b_encode_grb_byte_spi(uint8_t grb_byte, uint8_t *dst8, uint8_t codeword0, uint8_t codeword1);

#ifdef __cplusplus
}
#endif

#endif /* WS2812B_H */
