#include "es8311.h"

#include <string.h>

static bool es8311_cfg_valid(const es8311_config_t *cfg)
{
    if ((cfg == NULL) || (cfg->write == NULL) || (cfg->read == NULL) || (cfg->sample_rate_hz == 0U)) {
        return false;
    }
    return true;
}

es8311_status_t es8311_init_with_config(es8311_t *dev, const es8311_config_t *cfg)
{
    if ((dev == NULL) || !es8311_cfg_valid(cfg)) {
        return ES8311_ERROR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->write           = cfg->write;
    dev->read            = cfg->read;
    dev->write_read      = cfg->write_read;
    dev->delay_ms        = cfg->delay_ms;
    dev->i2c_addr7      = (cfg->i2c_addr7 != 0U) ? cfg->i2c_addr7 : ES8311_I2C_ADDR_7BIT_DEFAULT;
    dev->sample_rate_hz  = cfg->sample_rate_hz;
    dev->i2s_port        = cfg->i2s_port;
    dev->mode            = ES8311_MODE_IDLE;
    dev->initialized     = true;

    /* TODO(M2): 寄存器复位、时钟树、ADC/DAC 通路配置。 */
    return ES8311_OK;
}

es8311_status_t es8311_set_mode(es8311_t *dev, es8311_mode_t mode)
{
    if ((dev == NULL) || !dev->initialized) {
        return ES8311_ERROR_NOT_INIT;
    }
    dev->mode = mode;
    return ES8311_OK;
}

es8311_status_t es8311_start(es8311_t *dev)
{
    if ((dev == NULL) || !dev->initialized) {
        return ES8311_ERROR_NOT_INIT;
    }
    /* TODO(M5): 启动 I2S RX/TX 与 Codec 数据通路。 */
    return ES8311_OK;
}

es8311_status_t es8311_stop(es8311_t *dev)
{
    if ((dev == NULL) || !dev->initialized) {
        return ES8311_ERROR_NOT_INIT;
    }
    dev->mode = ES8311_MODE_IDLE;
    return ES8311_OK;
}

bool es8311_is_initialized(const es8311_t *dev)
{
    return (dev != NULL) && dev->initialized;
}
