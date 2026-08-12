#include "es8311.h"

#include "es8311_reg.h"

#include <string.h>

typedef struct {
    uint32_t mclk;
    uint32_t rate;
    uint8_t  pre_div;
    uint8_t  pre_multi;
    uint8_t  adc_div;
    uint8_t  dac_div;
    uint8_t  fs_mode;
    uint8_t  lrck_h;
    uint8_t  lrck_l;
    uint8_t  bclk_div;
    uint8_t  adc_osr;
    uint8_t  dac_osr;
} es8311_coeff_t;

/* 摘自 Espressif esp_codec_dev，覆盖 voice_hub 16 kHz 常用 MCLK。 */
static const es8311_coeff_t s_coeff_div[] = {
    {12288000U, 16000U, 0x03U, 0x01U, 0x01U, 0x01U, 0x00U, 0x00U, 0xFFU, 0x04U, 0x10U, 0x20U},
    {16384000U, 16000U, 0x04U, 0x01U, 0x01U, 0x01U, 0x00U, 0x00U, 0xFFU, 0x04U, 0x10U, 0x20U},
    {8192000U,  16000U, 0x02U, 0x01U, 0x01U, 0x01U, 0x00U, 0x00U, 0xFFU, 0x04U, 0x10U, 0x20U},
    {6144000U,  16000U, 0x03U, 0x02U, 0x01U, 0x01U, 0x00U, 0x00U, 0xFFU, 0x04U, 0x10U, 0x20U},
    {4096000U,  16000U, 0x01U, 0x01U, 0x01U, 0x01U, 0x00U, 0x00U, 0xFFU, 0x04U, 0x10U, 0x20U},
    {3072000U,  16000U, 0x03U, 0x04U, 0x01U, 0x01U, 0x00U, 0x00U, 0xFFU, 0x04U, 0x10U, 0x20U},
    {2048000U,  16000U, 0x01U, 0x02U, 0x01U, 0x01U, 0x00U, 0x00U, 0xFFU, 0x04U, 0x10U, 0x20U},
    {1536000U,  16000U, 0x03U, 0x08U, 0x01U, 0x01U, 0x00U, 0x00U, 0xFFU, 0x04U, 0x10U, 0x20U},
    {1024000U,  16000U, 0x01U, 0x04U, 0x01U, 0x01U, 0x00U, 0x00U, 0xFFU, 0x04U, 0x10U, 0x20U},
};

static bool es8311_cfg_valid(const es8311_config_t *cfg)
{
    if ((cfg == NULL) || (cfg->write == NULL) || (cfg->write_read == NULL) || (cfg->sample_rate_hz == 0U)) {
        return false;
    }
    return true;
}

static void es8311_delay(es8311_t *dev, uint32_t ms)
{
    if ((dev != NULL) && (dev->delay_ms != NULL)) {
        dev->delay_ms(ms);
    }
}

static es8311_status_t es8311_wr(es8311_t *dev, uint8_t reg, uint8_t val)
{
    uint8_t buf[2];

    if ((dev == NULL) || (dev->write == NULL)) {
        return ES8311_ERROR_PARAM;
    }

    buf[0] = reg;
    buf[1] = val;
    if (dev->write(dev->i2c_addr7, buf, 2U) != 0) {
        return ES8311_ERROR_I2C;
    }
    return ES8311_OK;
}

static es8311_status_t es8311_rd(es8311_t *dev, uint8_t reg, uint8_t *val)
{
    if ((dev == NULL) || (dev->write_read == NULL) || (val == NULL)) {
        return ES8311_ERROR_PARAM;
    }
    if (dev->write_read(dev->i2c_addr7, &reg, 1U, val, 1U) != 0) {
        return ES8311_ERROR_I2C;
    }
    return ES8311_OK;
}

static int es8311_get_coeff_index(uint32_t mclk, uint32_t rate)
{
    for (size_t i = 0U; i < (sizeof(s_coeff_div) / sizeof(s_coeff_div[0])); i++) {
        if ((s_coeff_div[i].rate == rate) && (s_coeff_div[i].mclk == mclk)) {
            return (int)i;
        }
    }
    return -1;
}

static es8311_status_t es8311_config_fmt_i2s(es8311_t *dev)
{
    es8311_status_t st;
    uint8_t         dac_iface = 0U;
    uint8_t         adc_iface = 0U;

    st = es8311_rd(dev, ES8311_SDPIN_REG09, &dac_iface);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_rd(dev, ES8311_SDPOUT_REG0A, &adc_iface);
    if (st != ES8311_OK) {
        return st;
    }

    dac_iface = (uint8_t)(dac_iface & 0xFCU);
    adc_iface = (uint8_t)(adc_iface & 0xFCU);
    dac_iface = (uint8_t)(dac_iface | 0x0CU);
    adc_iface = (uint8_t)(adc_iface | 0x0CU);

    st = es8311_wr(dev, ES8311_SDPIN_REG09, dac_iface);
    if (st != ES8311_OK) {
        return st;
    }
    return es8311_wr(dev, ES8311_SDPOUT_REG0A, adc_iface);
}

static es8311_status_t es8311_config_sample_rate(es8311_t *dev, uint32_t sample_rate_hz)
{
    es8311_status_t      st;
    const es8311_coeff_t *c;
    uint32_t             mclk_hz;
    int                  idx;
    uint8_t              regv;
    uint8_t              datmp;

    mclk_hz = sample_rate_hz * (uint32_t)dev->mclk_div;
    idx     = es8311_get_coeff_index(mclk_hz, sample_rate_hz);
    if (idx < 0) {
        return ES8311_ERROR_RATE;
    }
    c = &s_coeff_div[(size_t)idx];

    st = es8311_rd(dev, ES8311_CLK_MANAGER_REG02, &regv);
    if (st != ES8311_OK) {
        return st;
    }
    regv = (uint8_t)((regv & 0x07U) | (((c->pre_div - 1U) & 0x07U) << 5));
    datmp = 0U;
    switch (c->pre_multi) {
    case 1U:
        datmp = 0U;
        break;
    case 2U:
        datmp = 1U;
        break;
    case 4U:
        datmp = 2U;
        break;
    case 8U:
        datmp = 3U;
        break;
    default:
        break;
    }
    if (!dev->use_mclk) {
        datmp = 3U;
    }
    regv = (uint8_t)(regv | (datmp << 3));
    st   = es8311_wr(dev, ES8311_CLK_MANAGER_REG02, regv);
    if (st != ES8311_OK) {
        return st;
    }

    regv = (uint8_t)(((c->adc_div - 1U) << 4) | (c->dac_div - 1U));
    st   = es8311_wr(dev, ES8311_CLK_MANAGER_REG05, regv);
    if (st != ES8311_OK) {
        return st;
    }

    st = es8311_rd(dev, ES8311_CLK_MANAGER_REG03, &regv);
    if (st != ES8311_OK) {
        return st;
    }
    regv = (uint8_t)((regv & 0x80U) | ((c->fs_mode & 0x01U) << 6) | (c->adc_osr & 0x1FU));
    st   = es8311_wr(dev, ES8311_CLK_MANAGER_REG03, regv);
    if (st != ES8311_OK) {
        return st;
    }

    st = es8311_rd(dev, ES8311_CLK_MANAGER_REG04, &regv);
    if (st != ES8311_OK) {
        return st;
    }
    regv = (uint8_t)((regv & 0x80U) | (c->dac_osr & 0x1FU));
    st   = es8311_wr(dev, ES8311_CLK_MANAGER_REG04, regv);
    if (st != ES8311_OK) {
        return st;
    }

    st = es8311_rd(dev, ES8311_CLK_MANAGER_REG07, &regv);
    if (st != ES8311_OK) {
        return st;
    }
    regv = (uint8_t)((regv & 0xC0U) | (c->lrck_h & 0x3FU));
    st   = es8311_wr(dev, ES8311_CLK_MANAGER_REG07, regv);
    if (st != ES8311_OK) {
        return st;
    }

    st = es8311_wr(dev, ES8311_CLK_MANAGER_REG08, c->lrck_l);
    if (st != ES8311_OK) {
        return st;
    }

    st = es8311_rd(dev, ES8311_CLK_MANAGER_REG06, &regv);
    if (st != ES8311_OK) {
        return st;
    }
    regv = (uint8_t)(regv & 0xE0U);
    if (c->bclk_div < 19U) {
        regv = (uint8_t)(regv | ((c->bclk_div - 1U) & 0x1FU));
    } else {
        regv = (uint8_t)(regv | (c->bclk_div & 0x1FU));
    }
    return es8311_wr(dev, ES8311_CLK_MANAGER_REG06, regv);
}

static es8311_status_t es8311_hw_open(es8311_t *dev)
{
    es8311_status_t st;
    uint8_t         regv;

    st = es8311_wr(dev, ES8311_GPIO_REG44, 0x08U);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_wr(dev, ES8311_GPIO_REG44, 0x08U);
    if (st != ES8311_OK) {
        return st;
    }

    st = es8311_wr(dev, ES8311_CLK_MANAGER_REG01, 0x30U);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_wr(dev, ES8311_CLK_MANAGER_REG02, 0x00U);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_wr(dev, ES8311_CLK_MANAGER_REG03, 0x10U);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_wr(dev, ES8311_ADC_REG16, 0x24U);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_wr(dev, ES8311_CLK_MANAGER_REG04, 0x10U);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_wr(dev, ES8311_CLK_MANAGER_REG05, 0x00U);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_wr(dev, ES8311_SYSTEM_REG0B, 0x00U);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_wr(dev, ES8311_SYSTEM_REG0C, 0x00U);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_wr(dev, ES8311_SYSTEM_REG10, 0x1FU);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_wr(dev, ES8311_SYSTEM_REG11, 0x7FU);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_wr(dev, ES8311_RESET_REG00, 0x80U);
    if (st != ES8311_OK) {
        return st;
    }

    st = es8311_rd(dev, ES8311_RESET_REG00, &regv);
    if (st != ES8311_OK) {
        return st;
    }
    regv = (uint8_t)(regv & 0xBFU);
    st   = es8311_wr(dev, ES8311_RESET_REG00, regv);
    if (st != ES8311_OK) {
        return st;
    }

    regv = 0x3FU;
    if (dev->use_mclk) {
        regv = (uint8_t)(regv & 0x7FU);
    } else {
        regv = (uint8_t)(regv | 0x80U);
    }
    st = es8311_wr(dev, ES8311_CLK_MANAGER_REG01, regv);
    if (st != ES8311_OK) {
        return st;
    }

    st = es8311_wr(dev, ES8311_SYSTEM_REG13, 0x10U);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_wr(dev, ES8311_ADC_REG1B, 0x0AU);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_wr(dev, ES8311_ADC_REG1C, 0x6AU);
    if (st != ES8311_OK) {
        return st;
    }
    /* 0x08：ADCDAT=ADC+ADC（录音）。0x58 会混入 DACR，捕获-only 时右槽常为 0。 */
    st = es8311_wr(dev, ES8311_GPIO_REG44, 0x08U);
    if (st != ES8311_OK) {
        return st;
    }

    st = es8311_config_fmt_i2s(dev);
    if (st != ES8311_OK) {
        return st;
    }
    return es8311_config_sample_rate(dev, dev->sample_rate_hz);
}

static es8311_status_t es8311_hw_start(es8311_t *dev)
{
    es8311_status_t st;
    uint8_t         regv;
    uint8_t         dac_iface;
    uint8_t         adc_iface;

    st = es8311_rd(dev, ES8311_RESET_REG00, &regv);
    if (st != ES8311_OK) {
        return st;
    }
    regv = (uint8_t)((regv | 0x80U) & 0xBFU);
    st   = es8311_wr(dev, ES8311_RESET_REG00, regv);
    if (st != ES8311_OK) {
        return st;
    }

    regv = 0x3FU;
    if (dev->use_mclk) {
        regv = (uint8_t)(regv & 0x7FU);
    } else {
        regv = (uint8_t)(regv | 0x80U);
    }
    st = es8311_wr(dev, ES8311_CLK_MANAGER_REG01, regv);
    if (st != ES8311_OK) {
        return st;
    }

    st = es8311_rd(dev, ES8311_SDPIN_REG09, &dac_iface);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_rd(dev, ES8311_SDPOUT_REG0A, &adc_iface);
    if (st != ES8311_OK) {
        return st;
    }

    dac_iface = (uint8_t)(dac_iface & 0xBFU);
    adc_iface = (uint8_t)(adc_iface & 0xBFU);
    dac_iface = (uint8_t)(dac_iface | 0x40U);
    adc_iface = (uint8_t)(adc_iface | 0x40U);

    if ((dev->mode == ES8311_MODE_CAPTURE) || (dev->mode == ES8311_MODE_FULL_DUPLEX)) {
        adc_iface = (uint8_t)(adc_iface & 0xBFU);
    }
    if ((dev->mode == ES8311_MODE_PLAYBACK) || (dev->mode == ES8311_MODE_FULL_DUPLEX)) {
        dac_iface = (uint8_t)(dac_iface & 0xBFU);
    }

    st = es8311_wr(dev, ES8311_SDPIN_REG09, dac_iface);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_wr(dev, ES8311_SDPOUT_REG0A, adc_iface);
    if (st != ES8311_OK) {
        return st;
    }

    st = es8311_wr(dev, ES8311_ADC_REG17, 0xBFU);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_wr(dev, ES8311_SYSTEM_REG0E, 0x02U);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_wr(dev, ES8311_SYSTEM_REG12, 0x00U);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_wr(dev, ES8311_SYSTEM_REG14, 0x1AU);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_wr(dev, ES8311_SYSTEM_REG0D, 0x01U);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_wr(dev, ES8311_ADC_REG15, 0x40U);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_wr(dev, ES8311_DAC_REG37, 0x08U);
    if (st != ES8311_OK) {
        return st;
    }
    return es8311_wr(dev, ES8311_GP_REG45, 0x00U);
}

static es8311_status_t es8311_hw_suspend(es8311_t *dev)
{
    es8311_status_t st;

    /* 软停：只关 ADC/麦偏置，保留时钟与采样率配置，便于再次 start。 */
    st = es8311_wr(dev, ES8311_ADC_REG17, 0x00U);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_wr(dev, ES8311_SYSTEM_REG14, 0x00U);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_wr(dev, ES8311_SYSTEM_REG0E, 0xFFU);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_wr(dev, ES8311_ADC_REG15, 0x00U);
    if (st != ES8311_OK) {
        return st;
    }
    (void)dev;
    return ES8311_OK;
}

es8311_status_t es8311_init_with_config(es8311_t *dev, const es8311_config_t *cfg)
{
    es8311_status_t st;
    uint8_t           chip_id1 = 0U;
    uint8_t           chip_id2 = 0U;

    if ((dev == NULL) || !es8311_cfg_valid(cfg)) {
        return ES8311_ERROR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->write          = cfg->write;
    dev->read           = cfg->read;
    dev->write_read     = cfg->write_read;
    dev->delay_ms       = cfg->delay_ms;
    dev->i2c_addr7      = (cfg->i2c_addr7 != 0U) ? cfg->i2c_addr7 : ES8311_I2C_ADDR_7BIT_DEFAULT;
    dev->sample_rate_hz  = cfg->sample_rate_hz;
    dev->i2s_port        = cfg->i2s_port;
    dev->mclk_div        = (cfg->mclk_div != 0U) ? cfg->mclk_div : ES8311_MCLK_DIV_DEFAULT;
    dev->use_mclk        = cfg->use_mclk;
    dev->mode            = ES8311_MODE_IDLE;
    dev->initialized     = false;
    dev->running         = false;

    es8311_delay(dev, 20U);

    st = es8311_hw_open(dev);
    if (st != ES8311_OK) {
        return st;
    }

    st = es8311_rd(dev, ES8311_CHD1_REGFD, &chip_id1);
    if (st != ES8311_OK) {
        return st;
    }
    st = es8311_rd(dev, ES8311_CHD2_REGFE, &chip_id2);
    if (st != ES8311_OK) {
        return st;
    }
    if ((chip_id1 != 0x83U) || (chip_id2 != 0x11U)) {
        /* 部分模组 ID 读数异常但仍可工作；仅告警不硬失败 */
        /* 调用方可用日志区分；此处继续完成寄存器初始化 */
    }

    /* 注意：此时 initialized 仍为 false，不可调 es8311_set_dac_volume() */
    st = es8311_wr(dev, ES8311_DAC_REG32, 0xA0U);
    if (st != ES8311_OK) {
        return st;
    }

    st = es8311_wr(dev, ES8311_DAC_REG31, 0x00U);
    if (st != ES8311_OK) {
        return st;
    }

    dev->initialized = true;
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

es8311_status_t es8311_set_dac_volume(es8311_t *dev, uint8_t volume_reg)
{
    if ((dev == NULL) || !dev->initialized) {
        return ES8311_ERROR_NOT_INIT;
    }
    return es8311_wr(dev, ES8311_DAC_REG32, volume_reg);
}

es8311_status_t es8311_set_adc_volume(es8311_t *dev, uint8_t volume_reg)
{
    if ((dev == NULL) || !dev->initialized) {
        return ES8311_ERROR_NOT_INIT;
    }
    return es8311_wr(dev, ES8311_ADC_REG17, volume_reg);
}

es8311_status_t es8311_set_mic_gain(es8311_t *dev, uint8_t gain_0_to_7)
{
    uint8_t regv;

    if ((dev == NULL) || !dev->initialized) {
        return ES8311_ERROR_NOT_INIT;
    }
    if (gain_0_to_7 > 7U) {
        gain_0_to_7 = 7U;
    }
    /*
     * REG16：bit5=ADC_SYNC，bit2:0=ADC_SCALE(0..7 → 0..42dB)。
     * 与 Espressif init(0x24) 一致：保留 ADC_SYNC，只改 scale。
     */
    regv = (uint8_t)(0x20U | (gain_0_to_7 & 0x07U));
    return es8311_wr(dev, ES8311_ADC_REG16, regv);
}

es8311_status_t es8311_start(es8311_t *dev)
{
    es8311_status_t st;

    if ((dev == NULL) || !dev->initialized) {
        return ES8311_ERROR_NOT_INIT;
    }
    if (dev->running) {
        return ES8311_OK;
    }

    st = es8311_hw_start(dev);
    if (st != ES8311_OK) {
        return st;
    }

    dev->running = true;
    return ES8311_OK;
}

es8311_status_t es8311_stop(es8311_t *dev)
{
    es8311_status_t st;

    if ((dev == NULL) || !dev->initialized) {
        return ES8311_ERROR_NOT_INIT;
    }
    if (!dev->running) {
        dev->mode = ES8311_MODE_IDLE;
        return ES8311_OK;
    }

    st = es8311_hw_suspend(dev);
    if (st != ES8311_OK) {
        return st;
    }

    dev->running = false;
    dev->mode    = ES8311_MODE_IDLE;
    return ES8311_OK;
}

es8311_status_t es8311_read_reg(es8311_t *dev, uint8_t reg, uint8_t *val)
{
    if ((dev == NULL) || !dev->initialized) {
        return ES8311_ERROR_NOT_INIT;
    }
    return es8311_rd(dev, reg, val);
}

bool es8311_is_initialized(const es8311_t *dev)
{
    return (dev != NULL) && dev->initialized;
}
