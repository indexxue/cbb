/**
 * @file    ws2812b_esp32.h
 * @brief   ESP-IDF RMT TX backend for WS2812B (uses generic `ws2812b_t` pixel API).
 *
 * Build: add `ws2812b.c` + `ws2812b_esp32.c`, require ESP-IDF driver/rmt component.
 */

#ifndef WS2812B_ESP32_H
#define WS2812B_ESP32_H

#include "ws2812b.h"

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WS2812B_ESP32_DEFAULT_RESOLUTION_HZ 10000000u

typedef struct {
    gpio_num_t  gpio_num;
    uint16_t    num_leds;
    uint32_t    resolution_hz;
    size_t      mem_block_symbols;
    uint8_t     trans_queue_depth;
} ws2812b_esp32_config_t;

/**
 * @brief  Init pixel buffer + ESP-IDF RMT WS2812 timing encoder on `gpio_num`.
 */
esp_err_t ws2812b_esp32_init(ws2812b_t *dev, const ws2812b_esp32_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* WS2812B_ESP32_H */
