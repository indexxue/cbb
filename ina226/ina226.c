/**
 * @file    ina226.c
 * @brief   INA226 I2C 电流/功率监视驱动实现
 */

#include "ina226.h"

#include <stddef.h>

#define INA226_CFG_BRNG_SHIFT 13u
#define INA226_CFG_AVG_SHIFT  9u
#define INA226_CFG_VBUS_SHIFT 6u
#define INA226_CFG_VSH_SHIFT  3u

static ina226_status_t ina226_check_bus(ina226_t *dev)
{
    if (dev == NULL || !dev->initialized)
    {
        return INA226_ERROR_NOT_INIT;
    }
    if (dev->write == NULL)
    {
        return INA226_ERROR_PARAM;
    }
    if (dev->write_read == NULL && dev->read == NULL)
    {
        return INA226_ERROR_PARAM;
    }
    return INA226_OK;
}

uint16_t ina226_pack_config(const ina226_measure_cfg_t *cfg)
{
    if (cfg == NULL)
    {
        return 0u;
    }

    uint16_t value = 0u;
    value |= (uint16_t)((uint16_t)(cfg->bus_range & 1u) << INA226_CFG_BRNG_SHIFT);
    value |= (uint16_t)((uint16_t)(cfg->avg & 7u) << INA226_CFG_AVG_SHIFT);
    value |= (uint16_t)((uint16_t)(cfg->vbus_ct & 7u) << INA226_CFG_VBUS_SHIFT);
    value |= (uint16_t)((uint16_t)(cfg->vshunt_ct & 7u) << INA226_CFG_VSH_SHIFT);
    value |= (uint16_t)(cfg->mode & 7u);
    return value;
}

static ina226_status_t ina226_read_bytes(ina226_t *dev, uint8_t reg, uint8_t *rx, uint16_t len)
{
    ina226_status_t st = ina226_check_bus(dev);
    if (st != INA226_OK)
    {
        return st;
    }
    if (rx == NULL || len == 0u)
    {
        return INA226_ERROR_PARAM;
    }

    if (dev->write_read != NULL)
    {
        if (dev->write_read(dev->i2c_addr, &reg, 1u, rx, len) != 0)
        {
            return INA226_ERROR_I2C;
        }
        return INA226_OK;
    }

    if (dev->write(dev->i2c_addr, &reg, 1u) != 0)
    {
        return INA226_ERROR_I2C;
    }
    if (dev->read(dev->i2c_addr, rx, len) != 0)
    {
        return INA226_ERROR_I2C;
    }
    return INA226_OK;
}

static ina226_status_t ina226_write_bytes(ina226_t *dev, const uint8_t *tx, uint16_t len)
{
    ina226_status_t st = ina226_check_bus(dev);
    if (st != INA226_OK)
    {
        return st;
    }
    if (tx == NULL || len == 0u)
    {
        return INA226_ERROR_PARAM;
    }
    if (dev->write(dev->i2c_addr, tx, len) != 0)
    {
        return INA226_ERROR_I2C;
    }
    return INA226_OK;
}

ina226_status_t ina226_read_reg(ina226_t *dev, uint8_t reg, uint16_t *value)
{
    if (value == NULL)
    {
        return INA226_ERROR_PARAM;
    }

    uint8_t rx[2];
    ina226_status_t st = ina226_read_bytes(dev, reg, rx, 2u);
    if (st != INA226_OK)
    {
        return st;
    }

    *value = (uint16_t)(((uint16_t)rx[0] << 8) | rx[1]);
    return INA226_OK;
}

ina226_status_t ina226_write_reg(ina226_t *dev, uint8_t reg, uint16_t value)
{
    uint8_t buf[3];
    buf[0] = reg;
    buf[1] = (uint8_t)(value >> 8);
    buf[2] = (uint8_t)(value & 0xFFu);
    return ina226_write_bytes(dev, buf, 3u);
}

ina226_status_t ina226_reset(ina226_t *dev)
{
    return ina226_write_reg(dev, INA226_REG_CONFIG, INA226_CFG_RST);
}

ina226_status_t ina226_configure(ina226_t *dev, const ina226_measure_cfg_t *cfg)
{
    if (cfg == NULL)
    {
        return INA226_ERROR_PARAM;
    }
    return ina226_write_reg(dev, INA226_REG_CONFIG, ina226_pack_config(cfg));
}

ina226_status_t ina226_set_calibration(ina226_t *dev, float shunt_ohm, float max_expected_a)
{
    ina226_status_t st = ina226_check_bus(dev);
    if (st != INA226_OK)
    {
        return st;
    }
    if (shunt_ohm <= 0.0f || max_expected_a <= 0.0f)
    {
        return INA226_ERROR_RANGE;
    }

    float current_lsb = max_expected_a / 32768.0f;
    float denom = current_lsb * shunt_ohm;
    if (denom <= 0.0f)
    {
        return INA226_ERROR_CALIB;
    }

    float cal_f = INA226_CALIBRATION_CONST / denom;
    if (cal_f <= 0.0f || cal_f > 65535.0f)
    {
        return INA226_ERROR_CALIB;
    }

    uint16_t cal = (uint16_t)cal_f;
    st = ina226_write_reg(dev, INA226_REG_CALIBRATION, cal);
    if (st != INA226_OK)
    {
        return st;
    }

    dev->shunt_ohm   = shunt_ohm;
    dev->current_lsb = current_lsb;
    dev->power_lsb   = 25.0f * current_lsb;
    dev->cal_reg     = cal;
    return INA226_OK;
}

ina226_status_t ina226_init(ina226_t *dev, const ina226_config_t *cfg)
{
    if (dev == NULL || cfg == NULL || cfg->write == NULL)
    {
        return INA226_ERROR_PARAM;
    }
    if ((cfg->write_read == NULL) && (cfg->read == NULL))
    {
        return INA226_ERROR_PARAM;
    }
    if (cfg->shunt_ohm <= 0.0f || cfg->max_expected_a <= 0.0f)
    {
        return INA226_ERROR_RANGE;
    }

    dev->write       = cfg->write;
    dev->read        = cfg->read;
    dev->write_read  = cfg->write_read;
    dev->i2c_addr    = cfg->i2c_addr;
    dev->shunt_ohm   = cfg->shunt_ohm;
    dev->current_lsb = 0.0f;
    dev->power_lsb   = 0.0f;
    dev->cal_reg     = 0u;
    dev->initialized = true;

    ina226_status_t st = ina226_reset(dev);
    if (st != INA226_OK)
    {
        dev->initialized = false;
        return st;
    }

    st = ina226_configure(dev, &cfg->measure);
    if (st != INA226_OK)
    {
        dev->initialized = false;
        return st;
    }

    st = ina226_set_calibration(dev, cfg->shunt_ohm, cfg->max_expected_a);
    if (st != INA226_OK)
    {
        dev->initialized = false;
        return st;
    }

    return INA226_OK;
}

ina226_status_t ina226_read_shunt_raw(ina226_t *dev, int16_t *raw)
{
    if (raw == NULL)
    {
        return INA226_ERROR_PARAM;
    }
    uint16_t u16;
    ina226_status_t st = ina226_read_reg(dev, INA226_REG_SHUNT_VOLT, &u16);
    if (st == INA226_OK)
    {
        *raw = (int16_t)u16;
    }
    return st;
}

ina226_status_t ina226_read_bus_raw(ina226_t *dev, uint16_t *raw)
{
    if (raw == NULL)
    {
        return INA226_ERROR_PARAM;
    }
    return ina226_read_reg(dev, INA226_REG_BUS_VOLT, raw);
}

ina226_status_t ina226_read_current_raw(ina226_t *dev, int16_t *raw)
{
    if (raw == NULL)
    {
        return INA226_ERROR_PARAM;
    }
    uint16_t u16;
    ina226_status_t st = ina226_read_reg(dev, INA226_REG_CURRENT, &u16);
    if (st == INA226_OK)
    {
        *raw = (int16_t)u16;
    }
    return st;
}

ina226_status_t ina226_read_power_raw(ina226_t *dev, uint16_t *raw)
{
    if (raw == NULL)
    {
        return INA226_ERROR_PARAM;
    }
    return ina226_read_reg(dev, INA226_REG_POWER, raw);
}

ina226_status_t ina226_read_shunt_v(ina226_t *dev, float *v)
{
    if (v == NULL)
    {
        return INA226_ERROR_PARAM;
    }
    int16_t raw;
    ina226_status_t st = ina226_read_shunt_raw(dev, &raw);
    if (st != INA226_OK)
    {
        return st;
    }
    *v = (float)raw * 2.5e-6f;
    return INA226_OK;
}

ina226_status_t ina226_read_bus_v(ina226_t *dev, float *v)
{
    if (v == NULL)
    {
        return INA226_ERROR_PARAM;
    }
    uint16_t raw;
    ina226_status_t st = ina226_read_bus_raw(dev, &raw);
    if (st != INA226_OK)
    {
        return st;
    }
    *v = (float)(raw >> 3) * 1.25e-3f;
    return INA226_OK;
}

ina226_status_t ina226_read_current_a(ina226_t *dev, float *a)
{
    if (a == NULL)
    {
        return INA226_ERROR_PARAM;
    }
    if (dev == NULL || !dev->initialized || dev->current_lsb <= 0.0f)
    {
        return INA226_ERROR_CALIB;
    }

    int16_t raw;
    ina226_status_t st = ina226_read_current_raw(dev, &raw);
    if (st != INA226_OK)
    {
        return st;
    }
    *a = (float)raw * dev->current_lsb;
    return INA226_OK;
}

ina226_status_t ina226_read_power_w(ina226_t *dev, float *w)
{
    if (w == NULL)
    {
        return INA226_ERROR_PARAM;
    }
    if (dev == NULL || !dev->initialized || dev->power_lsb <= 0.0f)
    {
        return INA226_ERROR_CALIB;
    }

    uint16_t raw;
    ina226_status_t st = ina226_read_power_raw(dev, &raw);
    if (st != INA226_OK)
    {
        return st;
    }
    *w = (float)raw * dev->power_lsb;
    return INA226_OK;
}

float ina226_current_from_shunt(float shunt_v, float shunt_ohm)
{
    if (shunt_ohm <= 0.0f)
    {
        return 0.0f;
    }
    return shunt_v / shunt_ohm;
}

ina226_status_t ina226_read_all(ina226_t *dev, ina226_readings_t *out)
{
    if (out == NULL)
    {
        return INA226_ERROR_PARAM;
    }

    ina226_status_t st = ina226_read_shunt_v(dev, &out->shunt_v);
    if (st != INA226_OK)
    {
        return st;
    }
    st = ina226_read_bus_v(dev, &out->bus_v);
    if (st != INA226_OK)
    {
        return st;
    }
    st = ina226_read_current_a(dev, &out->current_a);
    if (st != INA226_OK)
    {
        return st;
    }
    return ina226_read_power_w(dev, &out->power_w);
}
