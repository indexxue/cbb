/**
 * @file    ws2812b_esp32.c
 * @brief   ESP-IDF RMT WS2812 backend; encoder timing from IDF led_strip example (Apache-2.0).
 */

#include "ws2812b_esp32.h"

#include <stddef.h>
#include <stdlib.h>

#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_check.h"

#define WS2812B_ESP32_CONTAINEROF(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))

typedef struct {
    rmt_encoder_t      base;
    rmt_encoder_t     *bytes_encoder;
    rmt_encoder_t     *copy_encoder;
    int                state;
    rmt_symbol_word_t  reset_code;
} ws2812b_esp32_encoder_t;

typedef struct {
    rmt_channel_handle_t  chan;
    rmt_encoder_handle_t  encoder;
    uint32_t              resolution_hz;
} ws2812b_esp32_ctx_t;

RMT_ENCODER_FUNC_ATTR
static size_t ws2812b_esp32_encode(rmt_encoder_t *encoder, rmt_channel_handle_t channel,
                                   const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state)
{
    ws2812b_esp32_encoder_t *enc = WS2812B_ESP32_CONTAINEROF(encoder, ws2812b_esp32_encoder_t, base);
    rmt_encode_state_t       session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t       state         = RMT_ENCODING_RESET;
    size_t                   encoded       = 0;

    switch (enc->state) {
    case 0:
        encoded += enc->bytes_encoder->encode(enc->bytes_encoder, channel, primary_data, data_size, &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            enc->state = 1;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            *ret_state = RMT_ENCODING_MEM_FULL;
            return encoded;
        }
        /* fall-through */
    case 1:
        encoded += enc->copy_encoder->encode(enc->copy_encoder, channel, &enc->reset_code,
                                           sizeof(enc->reset_code), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            enc->state = RMT_ENCODING_RESET;
            state |= RMT_ENCODING_COMPLETE;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
        }
        break;
    default:
        break;
    }
    *ret_state = state;
    return encoded;
}

static esp_err_t ws2812b_esp32_encoder_del(rmt_encoder_t *encoder)
{
    ws2812b_esp32_encoder_t *enc = WS2812B_ESP32_CONTAINEROF(encoder, ws2812b_esp32_encoder_t, base);
    rmt_del_encoder(enc->bytes_encoder);
    rmt_del_encoder(enc->copy_encoder);
    free(enc);
    return ESP_OK;
}

RMT_ENCODER_FUNC_ATTR
static esp_err_t ws2812b_esp32_encoder_reset(rmt_encoder_t *encoder)
{
    ws2812b_esp32_encoder_t *enc = WS2812B_ESP32_CONTAINEROF(encoder, ws2812b_esp32_encoder_t, base);
    rmt_encoder_reset(enc->bytes_encoder);
    rmt_encoder_reset(enc->copy_encoder);
    enc->state = RMT_ENCODING_RESET;
    return ESP_OK;
}

static esp_err_t ws2812b_esp32_new_encoder(uint32_t resolution_hz, rmt_encoder_handle_t *out)
{
    ws2812b_esp32_encoder_t *enc = rmt_alloc_encoder_mem(sizeof(ws2812b_esp32_encoder_t));
    if (enc == NULL) {
        return ESP_ERR_NO_MEM;
    }
    enc->base.encode = ws2812b_esp32_encode;
    enc->base.del    = ws2812b_esp32_encoder_del;
    enc->base.reset  = ws2812b_esp32_encoder_reset;

    const double us_per_tick = 1000000.0 / (double)resolution_hz;

    rmt_bytes_encoder_config_t bytes_cfg = {
        .bit0 = {
            .level0    = 1,
            .duration0 = (uint32_t)(0.3 / us_per_tick),
            .level1    = 0,
            .duration1 = (uint32_t)(0.9 / us_per_tick),
        },
        .bit1 = {
            .level0    = 1,
            .duration0 = (uint32_t)(0.9 / us_per_tick),
            .level1    = 0,
            .duration1 = (uint32_t)(0.3 / us_per_tick),
        },
        .flags.msb_first = 1,
    };

    esp_err_t err = rmt_new_bytes_encoder(&bytes_cfg, &enc->bytes_encoder);
    if (err != ESP_OK) {
        free(enc);
        return err;
    }

    rmt_copy_encoder_config_t copy_cfg = {};
    err                                = rmt_new_copy_encoder(&copy_cfg, &enc->copy_encoder);
    if (err != ESP_OK) {
        rmt_del_encoder(enc->bytes_encoder);
        free(enc);
        return err;
    }

    const uint32_t reset_ticks = (uint32_t)((uint64_t)resolution_hz * 50ULL / 1000000ULL / 2ULL);
    enc->reset_code            = (rmt_symbol_word_t){
        .level0 = 0, .duration0 = reset_ticks, .level1 = 0, .duration1 = reset_ticks,
    };

    *out = &enc->base;
    return ESP_OK;
}

static int ws2812b_esp32_transmit(const uint8_t *grb, size_t len, void *ctx)
{
    ws2812b_esp32_ctx_t *plat = (ws2812b_esp32_ctx_t *)ctx;
    rmt_transmit_config_t  tx_cfg = { .loop_count = 0 };

    if (plat == NULL || plat->chan == NULL || plat->encoder == NULL) {
        return -1;
    }
    if (rmt_transmit(plat->chan, plat->encoder, grb, len, &tx_cfg) != ESP_OK) {
        return -1;
    }
    return (rmt_tx_wait_all_done(plat->chan, -1) != ESP_OK) ? -1 : 0;
}

static void ws2812b_esp32_platform_deinit(void *platform)
{
    ws2812b_esp32_ctx_t *plat = (ws2812b_esp32_ctx_t *)platform;
    if (plat == NULL) {
        return;
    }
    if (plat->chan != NULL) {
        (void)rmt_disable(plat->chan);
        (void)rmt_del_channel(plat->chan);
    }
    if (plat->encoder != NULL) {
        (void)rmt_del_encoder(plat->encoder);
    }
    free(plat);
}

esp_err_t ws2812b_esp32_init(ws2812b_t *dev, const ws2812b_esp32_config_t *cfg)
{
    ESP_RETURN_ON_FALSE(dev != NULL && cfg != NULL, ESP_ERR_INVALID_ARG, "ws2812b", "null");
    ESP_RETURN_ON_FALSE(cfg->num_leds > 0 && cfg->gpio_num >= 0, ESP_ERR_INVALID_ARG, "ws2812b", "cfg");

    const uint32_t res_hz = (cfg->resolution_hz != 0u) ? cfg->resolution_hz : WS2812B_ESP32_DEFAULT_RESOLUTION_HZ;
    const size_t   blk    = (cfg->mem_block_symbols != 0u) ? cfg->mem_block_symbols : 64u;
    const uint8_t  qdepth = (cfg->trans_queue_depth != 0u) ? cfg->trans_queue_depth : 4u;

    ws2812b_esp32_ctx_t *plat = (ws2812b_esp32_ctx_t *)calloc(1, sizeof(*plat));
    ESP_RETURN_ON_FALSE(plat != NULL, ESP_ERR_NO_MEM, "ws2812b", "plat");

    rmt_tx_channel_config_t tx_cfg = {
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .gpio_num          = cfg->gpio_num,
        .mem_block_symbols = blk,
        .resolution_hz     = res_hz,
        .trans_queue_depth = qdepth,
    };

    esp_err_t err = rmt_new_tx_channel(&tx_cfg, &plat->chan);
    if (err != ESP_OK) {
        free(plat);
        return err;
    }

    err = ws2812b_esp32_new_encoder(res_hz, &plat->encoder);
    if (err != ESP_OK) {
        rmt_del_channel(plat->chan);
        free(plat);
        return err;
    }

    err = rmt_enable(plat->chan);
    if (err != ESP_OK) {
        rmt_del_encoder(plat->encoder);
        rmt_del_channel(plat->chan);
        free(plat);
        return err;
    }

    plat->resolution_hz = res_hz;

    if (ws2812b_init_user(dev, cfg->num_leds, ws2812b_esp32_transmit, plat) != WS2812B_OK) {
        ws2812b_esp32_platform_deinit(plat);
        return ESP_ERR_NO_MEM;
    }

    dev->platform         = plat;
    dev->platform_deinit  = ws2812b_esp32_platform_deinit;
    return ESP_OK;
}
