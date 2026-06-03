/**
 * @file    qmi8658a.c
 * @brief   Generic QMI8658A / QMI8658 6-axis IMU (I2C or SPI + delay callbacks, no MCU dependencies).
 */

#include "qmi8658a.h"

#define QMI8658A_REG_WHO_AM_I 0x00u
#define QMI8658A_REG_CTRL1    0x02u
#define QMI8658A_REG_CTRL2    0x03u
#define QMI8658A_REG_CTRL3    0x04u
#define QMI8658A_REG_CTRL7    0x08u
#define QMI8658A_REG_RESET    0x60u
#define QMI8658A_REG_TEMP_L   0x33u
#define QMI8658A_REG_AX_L     0x35u

#define QMI8658A_WHO_AM_I_VALUE  0x05u
#define QMI8658A_RESET_KEY       0xB0u
#define QMI8658A_CTRL1_ADDR_AI   (1u << 6)
#define QMI8658A_CTRL1_SIM        (1u << 7)
#define QMI8658A_DEFAULT_AODR     0x05u
#define QMI8658A_DEFAULT_GODR     0x05u
#define QMI8658A_CTRL7_AEN_GEN    0x03u

static bool qmi8658a_is_spi(const qmi8658a_t *dev)
{
    return (dev != NULL) && (dev->spi_xfer != NULL);
}

static qmi8658a_status_t qmi8658a_write_reg(qmi8658a_t *dev, uint8_t reg, uint8_t value)
{
    if (dev == NULL) {
        return QMI8658A_ERROR_PARAM;
    }

    if (qmi8658a_is_spi(dev)) {
        uint8_t tx[2] = { QMI8658A_SPI_ADDR_WRITE(reg), value };
        return (dev->spi_xfer(tx, NULL, 2u) != 0) ? QMI8658A_ERROR_I2C : QMI8658A_OK;
    }

    if (dev->write == NULL) {
        return QMI8658A_ERROR_NOT_INIT;
    }

    uint8_t buf[2] = { reg, value };
    return (dev->write(dev->address, buf, 2u) != 0) ? QMI8658A_ERROR_I2C : QMI8658A_OK;
}

static qmi8658a_status_t qmi8658a_read_regs(qmi8658a_t *dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (dev == NULL || data == NULL || len == 0u) {
        return QMI8658A_ERROR_PARAM;
    }

    if (qmi8658a_is_spi(dev)) {
        if (len > 15u) {
            return QMI8658A_ERROR_PARAM;
        }
        uint8_t tx[16];
        uint8_t rx[16];
        uint16_t i;
        const uint16_t total = (uint16_t)(1u + len);

        tx[0] = QMI8658A_SPI_ADDR_READ(reg);
        for (i = 1u; i < total; i++) {
            tx[i] = 0u;
        }
        if (dev->spi_xfer(tx, rx, total) != 0) {
            return QMI8658A_ERROR_I2C;
        }
        for (i = 0u; i < len; i++) {
            data[i] = rx[(uint16_t)(1u + i)];
        }
        return QMI8658A_OK;
    }

    if (dev->write_read != NULL) {
        return (dev->write_read(dev->address, &reg, 1u, data, len) != 0) ? QMI8658A_ERROR_I2C : QMI8658A_OK;
    }
    if (dev->write == NULL || dev->read == NULL) {
        return QMI8658A_ERROR_PARAM;
    }
    if (dev->write(dev->address, &reg, 1u) != 0) {
        return QMI8658A_ERROR_I2C;
    }
    return (dev->read(dev->address, data, len) != 0) ? QMI8658A_ERROR_I2C : QMI8658A_OK;
}

static uint8_t qmi8658a_pack_ctrl1(bool spi_3wire)
{
    uint8_t v = QMI8658A_CTRL1_ADDR_AI;
    if (spi_3wire) {
        v |= QMI8658A_CTRL1_SIM;
    }
    return v;
}

static uint8_t qmi8658a_pack_ctrl2(qmi8658a_accel_range_t range, uint8_t aodr)
{
    return (uint8_t)(((uint8_t)(range & 3u) << 4) | (aodr & 0x0Fu));
}

static uint8_t qmi8658a_pack_ctrl3(qmi8658a_gyro_range_t range, uint8_t godr)
{
    return (uint8_t)(((uint8_t)(range & 7u) << 4) | (godr & 0x0Fu));
}

static qmi8658a_status_t qmi8658a_apply_ctrl2_ctrl3(qmi8658a_t *dev)
{
    if (qmi8658a_write_reg(dev, QMI8658A_REG_CTRL7, 0x00u) != QMI8658A_OK) {
        return QMI8658A_ERROR_I2C;
    }
    if (qmi8658a_write_reg(dev, QMI8658A_REG_CTRL2,
                           qmi8658a_pack_ctrl2(dev->accel_range, QMI8658A_DEFAULT_AODR)) != QMI8658A_OK) {
        return QMI8658A_ERROR_I2C;
    }
    if (qmi8658a_write_reg(dev, QMI8658A_REG_CTRL3,
                           qmi8658a_pack_ctrl3(dev->gyro_range, QMI8658A_DEFAULT_GODR)) != QMI8658A_OK) {
        return QMI8658A_ERROR_I2C;
    }
    return (qmi8658a_write_reg(dev, QMI8658A_REG_CTRL7, QMI8658A_CTRL7_AEN_GEN) != QMI8658A_OK) ? QMI8658A_ERROR_I2C
                                                                                                : QMI8658A_OK;
}

static void qmi8658a_load_config(qmi8658a_t *dev, const qmi8658a_config_t *cfg)
{
    dev->write        = cfg->write;
    dev->read         = cfg->read;
    dev->write_read   = cfg->write_read;
    dev->spi_xfer     = cfg->spi_xfer;
    dev->delay_ms     = cfg->delay_ms;
    dev->address      = cfg->address;
    dev->spi_3wire    = cfg->spi_3wire;
    dev->accel_range  = cfg->accel_range;
    dev->gyro_range   = cfg->gyro_range;
    dev->initialized  = false;
}

qmi8658a_status_t qmi8658a_init_with_config(qmi8658a_t *dev, const qmi8658a_config_t *cfg)
{
    if (dev == NULL || cfg == NULL) {
        return QMI8658A_ERROR_PARAM;
    }
    if (cfg->spi_xfer == NULL &&
        (cfg->write == NULL || ((cfg->write_read == NULL) && (cfg->read == NULL)))) {
        return QMI8658A_ERROR_PARAM;
    }

    qmi8658a_load_config(dev, cfg);

    if (qmi8658a_write_reg(dev, QMI8658A_REG_CTRL7, 0x00u) != QMI8658A_OK) {
        return QMI8658A_ERROR_I2C;
    }
    if (qmi8658a_write_reg(dev, QMI8658A_REG_RESET, QMI8658A_RESET_KEY) != QMI8658A_OK) {
        return QMI8658A_ERROR_I2C;
    }
    if (dev->delay_ms != NULL) {
        dev->delay_ms(20u);
    }

    uint8_t id = 0u;
    if (qmi8658a_read_regs(dev, QMI8658A_REG_WHO_AM_I, &id, 1u) != QMI8658A_OK) {
        return QMI8658A_ERROR_I2C;
    }
    if (id != QMI8658A_WHO_AM_I_VALUE) {
        return QMI8658A_ERROR_ID;
    }
    if (qmi8658a_write_reg(dev, QMI8658A_REG_CTRL1, qmi8658a_pack_ctrl1(dev->spi_3wire)) != QMI8658A_OK) {
        return QMI8658A_ERROR_I2C;
    }
    if (qmi8658a_apply_ctrl2_ctrl3(dev) != QMI8658A_OK) {
        return QMI8658A_ERROR_I2C;
    }
    if (dev->delay_ms != NULL) {
        dev->delay_ms(100u);
    }

    dev->initialized = true;
    return QMI8658A_OK;
}

qmi8658a_status_t qmi8658a_init(qmi8658a_t *dev,
                                uint8_t address,
                                qmi8658a_i2c_write_t write,
                                qmi8658a_i2c_read_t read,
                                qmi8658a_delay_ms_t delay_ms)
{
    qmi8658a_config_t cfg = {0};
    cfg.write       = write;
    cfg.read        = read;
    cfg.delay_ms    = delay_ms;
    cfg.address     = address;
    cfg.accel_range = QMI8658A_ACCEL_RANGE_2G;
    cfg.gyro_range  = QMI8658A_GYRO_RANGE_2048DPS;
    return qmi8658a_init_with_config(dev, &cfg);
}

qmi8658a_status_t qmi8658a_init_spi(qmi8658a_t *dev,
                                   qmi8658a_spi_xfer_t xfer,
                                   qmi8658a_delay_ms_t delay_ms)
{
    qmi8658a_config_t cfg = {0};
    cfg.spi_xfer    = xfer;
    cfg.delay_ms    = delay_ms;
    cfg.accel_range = QMI8658A_ACCEL_RANGE_2G;
    cfg.gyro_range  = QMI8658A_GYRO_RANGE_2048DPS;
    return qmi8658a_init_with_config(dev, &cfg);
}

qmi8658a_status_t qmi8658a_set_accel_range(qmi8658a_t *dev, qmi8658a_accel_range_t range)
{
    if (dev == NULL || !dev->initialized) {
        return QMI8658A_ERROR_NOT_INIT;
    }
    if (range > QMI8658A_ACCEL_RANGE_16G) {
        return QMI8658A_ERROR_PARAM;
    }
    dev->accel_range = range;
    return qmi8658a_apply_ctrl2_ctrl3(dev);
}

qmi8658a_status_t qmi8658a_set_gyro_range(qmi8658a_t *dev, qmi8658a_gyro_range_t range)
{
    if (dev == NULL || !dev->initialized) {
        return QMI8658A_ERROR_NOT_INIT;
    }
    if (range > QMI8658A_GYRO_RANGE_2048DPS) {
        return QMI8658A_ERROR_PARAM;
    }
    dev->gyro_range = range;
    return qmi8658a_apply_ctrl2_ctrl3(dev);
}

qmi8658a_status_t qmi8658a_read_raw(qmi8658a_t *dev,
                                    int16_t *ax,
                                    int16_t *ay,
                                    int16_t *az,
                                    int16_t *gx,
                                    int16_t *gy,
                                    int16_t *gz)
{
    if (dev == NULL || !dev->initialized) {
        return QMI8658A_ERROR_NOT_INIT;
    }
    if (ax == NULL || ay == NULL || az == NULL || gx == NULL || gy == NULL || gz == NULL) {
        return QMI8658A_ERROR_PARAM;
    }

    uint8_t buf[12];
    if (qmi8658a_read_regs(dev, QMI8658A_REG_AX_L, buf, sizeof(buf)) != QMI8658A_OK) {
        return QMI8658A_ERROR_I2C;
    }

    *ax = (int16_t)(((uint16_t)buf[1] << 8) | buf[0]);
    *ay = (int16_t)(((uint16_t)buf[3] << 8) | buf[2]);
    *az = (int16_t)(((uint16_t)buf[5] << 8) | buf[4]);
    *gx = (int16_t)(((uint16_t)buf[7] << 8) | buf[6]);
    *gy = (int16_t)(((uint16_t)buf[9] << 8) | buf[8]);
    *gz = (int16_t)(((uint16_t)buf[11] << 8) | buf[10]);
    return QMI8658A_OK;
}

qmi8658a_status_t qmi8658a_read_temperature(qmi8658a_t *dev, float *temp_c)
{
    if (dev == NULL || !dev->initialized || temp_c == NULL) {
        return QMI8658A_ERROR_NOT_INIT;
    }

    uint8_t buf[2];
    if (qmi8658a_read_regs(dev, QMI8658A_REG_TEMP_L, buf, 2u) != QMI8658A_OK) {
        return QMI8658A_ERROR_I2C;
    }

    int16_t raw = (int16_t)(((uint16_t)buf[1] << 8) | buf[0]);
    *temp_c     = (float)raw / 256.0f;
    return QMI8658A_OK;
}
