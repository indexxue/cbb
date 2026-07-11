/**
 * @file    qmc5883p.c
 * @brief   QMC5883P 三轴磁力计（I2C 回调注入，寄存器表按 QST 数据手册）
 */

#include "qmc5883p.h"

#include <stddef.h>

#define QMC5883P_REG_CHIP_ID  0x00u
#define QMC5883P_REG_DATA_X_L 0x01u
#define QMC5883P_REG_STATUS   0x09u
#define QMC5883P_REG_CTRL1    0x0Au
#define QMC5883P_REG_CTRL2    0x0Bu
#define QMC5883P_REG_SIGN     0x29u

#define QMC5883P_CHIP_ID_VALUE 0x80u
#define QMC5883P_STATUS_DRDY   (1u << 0)

#define QMC5883P_CTRL2_SOFT_RST 0x80u
#define QMC5883P_SIGN_DEFAULT   0x06u
#define QMC5883P_CTRL2_DEFAULT  0x08u
#define QMC5883P_CTRL1_NORMAL   0xCDu

#define QMC5883P_DATA_READY_POLL_MS 10u
#define QMC5883P_DATA_READY_RETRIES 50u

static qmc5883p_status_t qmc5883p_write_reg(qmc5883p_t *dev, uint8_t reg, uint8_t value)
{
    uint8_t buf[2];

    if ((dev == NULL) || (dev->write == NULL)) {
        return QMC5883P_ERROR_NOT_INIT;
    }

    buf[0] = reg;
    buf[1] = value;
    if (dev->write(dev->address, buf, 2u) != 0) {
        return QMC5883P_ERROR_I2C;
    }
    return QMC5883P_OK;
}

static qmc5883p_status_t qmc5883p_read_regs(qmc5883p_t *dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    if ((dev == NULL) || (dev->read == NULL) || (data == NULL) || (len == 0u)) {
        return QMC5883P_ERROR_PARAM;
    }

    if (dev->read(dev->address, reg, data, len) != 0) {
        return QMC5883P_ERROR_I2C;
    }
    return QMC5883P_OK;
}

static qmc5883p_status_t qmc5883p_init_core(qmc5883p_t *dev)
{
    uint8_t chip_id = 0u;

    if (qmc5883p_read_regs(dev, QMC5883P_REG_CHIP_ID, &chip_id, 1u) != QMC5883P_OK) {
        return QMC5883P_ERROR_I2C;
    }
    if (!dev->skip_id_check && (chip_id != QMC5883P_CHIP_ID_VALUE)) {
        return QMC5883P_ERROR_ID;
    }

    if (qmc5883p_write_reg(dev, QMC5883P_REG_CTRL2, QMC5883P_CTRL2_SOFT_RST) != QMC5883P_OK) {
        return QMC5883P_ERROR_I2C;
    }
    if (dev->delay_ms != NULL) {
        dev->delay_ms(10u);
    }

    if (qmc5883p_write_reg(dev, QMC5883P_REG_SIGN, QMC5883P_SIGN_DEFAULT) != QMC5883P_OK) {
        return QMC5883P_ERROR_I2C;
    }
    if (qmc5883p_write_reg(dev, QMC5883P_REG_CTRL2, QMC5883P_CTRL2_DEFAULT) != QMC5883P_OK) {
        return QMC5883P_ERROR_I2C;
    }
    if (qmc5883p_write_reg(dev, QMC5883P_REG_CTRL1, QMC5883P_CTRL1_NORMAL) != QMC5883P_OK) {
        return QMC5883P_ERROR_I2C;
    }

    if (dev->delay_ms != NULL) {
        dev->delay_ms(10u);
    }

    dev->initialized = true;
    return QMC5883P_OK;
}

qmc5883p_status_t qmc5883p_init_with_config(qmc5883p_t *dev, const qmc5883p_config_t *cfg)
{
    if ((dev == NULL) || (cfg == NULL) || (cfg->write == NULL) || (cfg->read == NULL)) {
        return QMC5883P_ERROR_PARAM;
    }

    dev->write         = cfg->write;
    dev->read          = cfg->read;
    dev->delay_ms      = cfg->delay_ms;
    dev->address       = cfg->address;
    dev->skip_id_check = cfg->skip_id_check;
    dev->initialized   = false;

    return qmc5883p_init_core(dev);
}

qmc5883p_status_t qmc5883p_init(qmc5883p_t *dev,
                                uint8_t address,
                                qmc5883p_i2c_write_t write,
                                qmc5883p_i2c_read_t read,
                                qmc5883p_delay_ms_t delay_ms)
{
    qmc5883p_config_t cfg = {0};

    cfg.write    = write;
    cfg.read     = read;
    cfg.delay_ms = delay_ms;
    cfg.address  = address;
    return qmc5883p_init_with_config(dev, &cfg);
}

static qmc5883p_status_t qmc5883p_decode_raw(const uint8_t *buf, int16_t *mx, int16_t *my, int16_t *mz)
{
    if ((buf == NULL) || (mx == NULL) || (my == NULL) || (mz == NULL)) {
        return QMC5883P_ERROR_PARAM;
    }

    *mx = (int16_t)(((uint16_t)buf[1] << 8) | buf[0]);
    *my = (int16_t)(((uint16_t)buf[3] << 8) | buf[2]);
    *mz = (int16_t)(((uint16_t)buf[5] << 8) | buf[4]);
    return QMC5883P_OK;
}

qmc5883p_status_t qmc5883p_read_raw_nowait(qmc5883p_t *dev, int16_t *mx, int16_t *my, int16_t *mz)
{
    uint8_t buf[6];

    if ((dev == NULL) || !dev->initialized) {
        return QMC5883P_ERROR_NOT_INIT;
    }
    if ((mx == NULL) || (my == NULL) || (mz == NULL)) {
        return QMC5883P_ERROR_PARAM;
    }

    /* 连续模式下寄存器始终为最近一次转换结果，勿阻塞等待 DRDY */
    if (qmc5883p_read_regs(dev, QMC5883P_REG_DATA_X_L, buf, sizeof(buf)) != QMC5883P_OK) {
        return QMC5883P_ERROR_I2C;
    }

    return qmc5883p_decode_raw(buf, mx, my, mz);
}

qmc5883p_status_t qmc5883p_read_raw(qmc5883p_t *dev, int16_t *mx, int16_t *my, int16_t *mz)
{
    uint8_t status = 0u;
    uint8_t buf[6];
    uint32_t retry;

    if ((dev == NULL) || !dev->initialized) {
        return QMC5883P_ERROR_NOT_INIT;
    }
    if ((mx == NULL) || (my == NULL) || (mz == NULL)) {
        return QMC5883P_ERROR_PARAM;
    }

    for (retry = 0u; retry < QMC5883P_DATA_READY_RETRIES; retry++) {
        if (qmc5883p_read_regs(dev, QMC5883P_REG_STATUS, &status, 1u) != QMC5883P_OK) {
            return QMC5883P_ERROR_I2C;
        }
        if ((status & QMC5883P_STATUS_DRDY) != 0u) {
            break;
        }
        if (dev->delay_ms != NULL) {
            dev->delay_ms(QMC5883P_DATA_READY_POLL_MS);
        }
    }

    if (qmc5883p_read_regs(dev, QMC5883P_REG_DATA_X_L, buf, sizeof(buf)) != QMC5883P_OK) {
        return QMC5883P_ERROR_I2C;
    }

    return qmc5883p_decode_raw(buf, mx, my, mz);
}
