/**
 * @file es8311.h
 * @brief ES8311 音频 Codec（I2C 控制 + I2S 从模式，回调注入）。
 */

#ifndef ES8311_H
#define ES8311_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 7-bit I2C 地址，常见 0x18（CE=0）。 */
#define ES8311_I2C_ADDR_7BIT_DEFAULT (0x18U)

typedef int (*es8311_i2c_write_t)(uint8_t addr7, const uint8_t *data, uint16_t len);
typedef int (*es8311_i2c_read_t)(uint8_t addr7, uint8_t *data, uint16_t len);
typedef int (*es8311_i2c_write_read_t)(uint8_t addr7,
                                         const uint8_t *write_data,
                                         uint16_t write_len,
                                         uint8_t *read_data,
                                         uint16_t read_len);
typedef void (*es8311_delay_ms_t)(uint32_t ms);

typedef enum {
    ES8311_OK = 0,
    ES8311_ERROR_PARAM,
    ES8311_ERROR_NOT_INIT,
    ES8311_ERROR_I2C,
    ES8311_ERROR_RATE,
} es8311_status_t;

typedef enum {
    ES8311_MODE_IDLE = 0,
    ES8311_MODE_PLAYBACK,
    ES8311_MODE_CAPTURE,
    ES8311_MODE_FULL_DUPLEX,
} es8311_mode_t;

typedef struct {
    es8311_i2c_write_t      write;
    es8311_i2c_read_t       read;
    es8311_i2c_write_read_t write_read;
    es8311_delay_ms_t       delay_ms;
    uint8_t                 i2c_addr7;
    uint32_t                sample_rate_hz;
    int                     i2s_port;
    /** MCLK = sample_rate_hz * mclk_div；0 表示使用 256。 */
    uint16_t                mclk_div;
    bool                    use_mclk;
} es8311_config_t;

typedef struct {
    es8311_i2c_write_t      write;
    es8311_i2c_read_t       read;
    es8311_i2c_write_read_t write_read;
    es8311_delay_ms_t       delay_ms;
    uint8_t                 i2c_addr7;
    uint32_t                sample_rate_hz;
    uint16_t                mclk_div;
    bool                    use_mclk;
    int                     i2s_port;
    es8311_mode_t           mode;
    bool                    initialized;
    bool                    running;
} es8311_t;

es8311_status_t es8311_init_with_config(es8311_t *dev, const es8311_config_t *cfg);
es8311_status_t es8311_set_mode(es8311_t *dev, es8311_mode_t mode);
es8311_status_t es8311_set_dac_volume(es8311_t *dev, uint8_t volume_reg);
es8311_status_t es8311_start(es8311_t *dev);
es8311_status_t es8311_stop(es8311_t *dev);
bool es8311_is_initialized(const es8311_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* ES8311_H */
