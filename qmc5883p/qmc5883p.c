/**
 * @file    qmc5883p.c
 * @brief   Generic QMC5883P 3-axis magnetometer (I2C + delay callbacks, no MCU dependencies).
 */

#include "qmc5883p.h"

#define QMC5883P_REG_CHIPID   0x00u
#define QMC5883P_REG_XOUT_L   0x01u
#define QMC5883P_REG_STATUS   0x09u
#define QMC5883P_REG_CTRL1    0x0Au
#define QMC5883P_REG_CTRL2    0x0Bu
#define QMC5883P_REG_XYZ_SIGN 0x29u

#define QMC5883P_CHIPID_VALUE      0x80u
#define QMC5883P_XYZ_SIGN_VALUE    0x06u

#define QMC5883P_STATUS_DRDY 0x01u
#define QMC5883P_STATUS_OVFL 0x02u

#define QMC5883P_CTRL2_SOFT_RST 0x80u

static qmc5883p_status_t qmc5883p_write_reg(qmc5883p_t *dev, uint8_t reg, uint8_t value)
{
    if (dev == NULL || dev->write == NULL) {
        return QMC5883P_ERROR_NOT_INIT;
    }

    uint8_t buf[2];
    buf[0] = reg;
    buf[1] = value;

    if (dev->write(dev->address, buf, 2u) != 0) {
        return QMC5883P_ERROR_I2C;
    }
    return QMC5883P_OK;
}

static qmc5883p_status_t qmc5883p_read_regs(qmc5883p_t *dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (dev == NULL || data == NULL || len == 0u) {
        return QMC5883P_ERROR_PARAM;
    }

    if (dev->write_read != NULL) {
        if (dev->write_read(dev->address, &reg, 1u, data, len) != 0) {
            return QMC5883P_ERROR_I2C;
        }
        return QMC5883P_OK;
    }

    if (dev->write == NULL || dev->read == NULL) {
        return QMC5883P_ERROR_PARAM;
    }

    if (dev->write(dev->address, &reg, 1u) != 0) {
        return QMC5883P_ERROR_I2C;
    }
    if (dev->read(dev->address, data, len) != 0) {
        return QMC5883P_ERROR_I2C;
    }
    return QMC5883P_OK;
}

static uint8_t qmc5883p_pack_ctrl1(const qmc5883p_t *dev)
{
    return (uint8_t)(((uint8_t)dev->dsr & 3u) << 6) | (((uint8_t)dev->osr & 3u) << 4) |
           (((uint8_t)dev->odr & 3u) << 2) | ((uint8_t)dev->mode & 3u);
}

static uint8_t qmc5883p_pack_ctrl2(const qmc5883p_t *dev)
{
    return (uint8_t)(((uint8_t)dev->range & 3u) << 2) | ((uint8_t)dev->setreset & 3u);
}

static float qmc5883p_lsb_per_gauss(qmc5883p_range_t range)
{
    switch (range) {
    case QMC5883P_RANGE_30G:
        return 1000.0f;
    case QMC5883P_RANGE_12G:
        return 2500.0f;
    case QMC5883P_RANGE_8G:
        return 3750.0f;
    case QMC5883P_RANGE_2G:
    default:
        return 15000.0f;
    }
}

static qmc5883p_status_t qmc5883p_apply_config(qmc5883p_t *dev)
{
    if (qmc5883p_write_reg(dev, QMC5883P_REG_CTRL2, qmc5883p_pack_ctrl2(dev)) != QMC5883P_OK) {
        return QMC5883P_ERROR_I2C;
    }
    if (qmc5883p_write_reg(dev, QMC5883P_REG_CTRL1, qmc5883p_pack_ctrl1(dev)) != QMC5883P_OK) {
        return QMC5883P_ERROR_I2C;
    }
    return QMC5883P_OK;
}

static void qmc5883p_load_config(qmc5883p_t *dev, const qmc5883p_config_t *cfg)
{
    dev->write       = cfg->write;
    dev->read        = cfg->read;
    dev->write_read  = cfg->write_read;
    dev->delay_ms    = cfg->delay_ms;
    dev->address     = cfg->address;
    dev->mode        = cfg->mode;
    dev->odr         = cfg->odr;
    dev->osr         = cfg->osr;
    dev->dsr         = cfg->dsr;
    dev->range       = cfg->range;
    dev->setreset    = cfg->setreset;
    dev->initialized = false;
}

qmc5883p_status_t qmc5883p_init_with_config(qmc5883p_t *dev, const qmc5883p_config_t *cfg)
{
    if (dev == NULL || cfg == NULL || cfg->write == NULL) {
        return QMC5883P_ERROR_PARAM;
    }
    if ((cfg->write_read == NULL) && (cfg->read == NULL)) {
        return QMC5883P_ERROR_PARAM;
    }

    qmc5883p_load_config(dev, cfg);

    uint8_t id = 0u;
    if (qmc5883p_read_regs(dev, QMC5883P_REG_CHIPID, &id, 1u) != QMC5883P_OK) {
        return QMC5883P_ERROR_I2C;
    }
    if (id != QMC5883P_CHIPID_VALUE) {
        return QMC5883P_ERROR_ID;
    }

    if (qmc5883p_write_reg(dev, QMC5883P_REG_XYZ_SIGN, QMC5883P_XYZ_SIGN_VALUE) != QMC5883P_OK) {
        return QMC5883P_ERROR_I2C;
    }

    if (qmc5883p_apply_config(dev) != QMC5883P_OK) {
        return QMC5883P_ERROR_I2C;
    }

    if (dev->delay_ms != NULL) {
        dev->delay_ms(10u);
    }

    dev->initialized = true;
    return QMC5883P_OK;
}

qmc5883p_status_t qmc5883p_init(qmc5883p_t *dev,
                                qmc5883p_i2c_write_t write,
                                qmc5883p_i2c_read_t read,
                                qmc5883p_delay_ms_t delay_ms)
{
    qmc5883p_config_t cfg = {0};
    cfg.write      = write;
    cfg.read       = read;
    cfg.write_read = NULL;
    cfg.delay_ms   = delay_ms;
    cfg.address    = QMC5883P_DEFAULT_I2C_ADDR;
    cfg.mode       = QMC5883P_MODE_NORMAL;
    cfg.odr        = QMC5883P_ODR_100HZ;
    cfg.osr        = QMC5883P_OSR_8;
    cfg.dsr        = QMC5883P_DSR_8;
    cfg.range      = QMC5883P_RANGE_8G;
    cfg.setreset   = QMC5883P_SETRESET_ON;
    return qmc5883p_init_with_config(dev, &cfg);
}

qmc5883p_status_t qmc5883p_soft_reset(qmc5883p_t *dev)
{
    if (dev == NULL || dev->write == NULL) {
        return QMC5883P_ERROR_NOT_INIT;
    }

    if (qmc5883p_write_reg(dev, QMC5883P_REG_CTRL2, QMC5883P_CTRL2_SOFT_RST) != QMC5883P_OK) {
        return QMC5883P_ERROR_I2C;
    }

    if (dev->delay_ms != NULL) {
        dev->delay_ms(50u);
    }

    dev->initialized = false;

    uint8_t id = 0u;
    if (qmc5883p_read_regs(dev, QMC5883P_REG_CHIPID, &id, 1u) != QMC5883P_OK) {
        return QMC5883P_ERROR_I2C;
    }
    if (id != QMC5883P_CHIPID_VALUE) {
        return QMC5883P_ERROR_ID;
    }

    return QMC5883P_OK;
}

qmc5883p_status_t qmc5883p_set_mode(qmc5883p_t *dev, qmc5883p_mode_t mode)
{
    if (dev == NULL || !dev->initialized) {
        return QMC5883P_ERROR_NOT_INIT;
    }
    if (mode > QMC5883P_MODE_CONTINUOUS) {
        return QMC5883P_ERROR_PARAM;
    }

    dev->mode = mode;
    return qmc5883p_apply_config(dev);
}

qmc5883p_status_t qmc5883p_set_odr(qmc5883p_t *dev, qmc5883p_odr_t odr)
{
    if (dev == NULL || !dev->initialized) {
        return QMC5883P_ERROR_NOT_INIT;
    }
    if (odr > QMC5883P_ODR_200HZ) {
        return QMC5883P_ERROR_PARAM;
    }

    dev->odr = odr;
    return qmc5883p_apply_config(dev);
}

qmc5883p_status_t qmc5883p_set_range(qmc5883p_t *dev, qmc5883p_range_t range)
{
    if (dev == NULL || !dev->initialized) {
        return QMC5883P_ERROR_NOT_INIT;
    }
    if (range > QMC5883P_RANGE_2G) {
        return QMC5883P_ERROR_PARAM;
    }

    dev->range = range;
    return qmc5883p_apply_config(dev);
}

qmc5883p_status_t qmc5883p_read_raw(qmc5883p_t *dev, int16_t *mx, int16_t *my, int16_t *mz)
{
    if (dev == NULL || !dev->initialized) {
        return QMC5883P_ERROR_NOT_INIT;
    }
    if (mx == NULL || my == NULL || mz == NULL) {
        return QMC5883P_ERROR_PARAM;
    }

    uint8_t buf[6];
    if (qmc5883p_read_regs(dev, QMC5883P_REG_XOUT_L, buf, sizeof(buf)) != QMC5883P_OK) {
        return QMC5883P_ERROR_I2C;
    }

    *mx = (int16_t)(((uint16_t)buf[1] << 8) | buf[0]);
    *my = (int16_t)(((uint16_t)buf[3] << 8) | buf[2]);
    *mz = (int16_t)(((uint16_t)buf[5] << 8) | buf[4]);

    return QMC5883P_OK;
}

qmc5883p_status_t qmc5883p_read_gauss(qmc5883p_t *dev, float *mx, float *my, float *mz)
{
    if (dev == NULL || !dev->initialized || mx == NULL || my == NULL || mz == NULL) {
        return QMC5883P_ERROR_NOT_INIT;
    }

    int16_t raw_x = 0;
    int16_t raw_y = 0;
    int16_t raw_z = 0;
    qmc5883p_status_t st = qmc5883p_read_raw(dev, &raw_x, &raw_y, &raw_z);
    if (st != QMC5883P_OK) {
        return st;
    }

    float scale = qmc5883p_lsb_per_gauss(dev->range);
    *mx         = (float)raw_x / scale;
    *my         = (float)raw_y / scale;
    *mz         = (float)raw_z / scale;

    return QMC5883P_OK;
}

qmc5883p_status_t qmc5883p_data_ready(qmc5883p_t *dev, bool *ready)
{
    if (dev == NULL || !dev->initialized || ready == NULL) {
        return QMC5883P_ERROR_NOT_INIT;
    }

    uint8_t status = 0u;
    if (qmc5883p_read_regs(dev, QMC5883P_REG_STATUS, &status, 1u) != QMC5883P_OK) {
        return QMC5883P_ERROR_I2C;
    }

    *ready = (status & QMC5883P_STATUS_DRDY) != 0u;
    return QMC5883P_OK;
}

qmc5883p_status_t qmc5883p_overflow(qmc5883p_t *dev, bool *overflow)
{
    if (dev == NULL || !dev->initialized || overflow == NULL) {
        return QMC5883P_ERROR_NOT_INIT;
    }

    uint8_t status = 0u;
    if (qmc5883p_read_regs(dev, QMC5883P_REG_STATUS, &status, 1u) != QMC5883P_OK) {
        return QMC5883P_ERROR_I2C;
    }

    *overflow = (status & QMC5883P_STATUS_OVFL) != 0u;
    return QMC5883P_OK;
}
