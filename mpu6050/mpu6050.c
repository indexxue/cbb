/**
 * @file    mpu6050.c
 * @brief   MPU6050 六轴 IMU（I2C 回调注入）
 */

#include "mpu6050.h"

#include <stddef.h>

#define MPU6050_REG_SMPLRT_DIV   0x19u
#define MPU6050_REG_CONFIG       0x1Au
#define MPU6050_REG_GYRO_CONFIG  0x1Bu
#define MPU6050_REG_ACCEL_CONFIG 0x1Cu
#define MPU6050_REG_WHO_AM_I     0x75u
#define MPU6050_REG_PWR_MGMT_1   0x6Bu
#define MPU6050_REG_ACCEL_XOUT_H 0x3Bu

/** DLPF_CFG=0：陀螺 260Hz 带宽，响应最快；±250°/s、±2g 与 attitude.c 换算一致 */
#define MPU6050_DLPF_CFG_FAST    0x00u
#define MPU6050_GYRO_FS_250DPS   0x00u
#define MPU6050_ACCEL_FS_2G      0x00u

#define MPU6050_WHO_AM_I_VALUE 0x68u

static mpu6050_status_t mpu6050_write_reg(mpu6050_t *dev, uint8_t reg, uint8_t value)
{
    uint8_t buf[2];

    if ((dev == NULL) || (dev->write == NULL)) {
        return MPU6050_ERROR_NOT_INIT;
    }

    buf[0] = reg;
    buf[1] = value;
    if (dev->write(dev->address, buf, 2u) != 0) {
        return MPU6050_ERROR_I2C;
    }
    return MPU6050_OK;
}

static mpu6050_status_t mpu6050_read_regs(mpu6050_t *dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    if ((dev == NULL) || (dev->read == NULL) || (data == NULL) || (len == 0u)) {
        return MPU6050_ERROR_PARAM;
    }

    if (dev->read(dev->address, reg, data, len) != 0) {
        return MPU6050_ERROR_I2C;
    }
    return MPU6050_OK;
}

static mpu6050_status_t mpu6050_init_core(mpu6050_t *dev)
{
    uint8_t id = 0u;

    if (mpu6050_read_regs(dev, MPU6050_REG_WHO_AM_I, &id, 1u) != MPU6050_OK) {
        return MPU6050_ERROR_I2C;
    }
    if (!dev->skip_id_check && (id != MPU6050_WHO_AM_I_VALUE)) {
        return MPU6050_ERROR_ID;
    }

    if (mpu6050_write_reg(dev, MPU6050_REG_PWR_MGMT_1, 0x00u) != MPU6050_OK) {
        return MPU6050_ERROR_I2C;
    }

    if (dev->delay_ms != NULL) {
        dev->delay_ms(100u);
    }

    if (mpu6050_write_reg(dev, MPU6050_REG_SMPLRT_DIV, 0x00u) != MPU6050_OK) {
        return MPU6050_ERROR_I2C;
    }
    if (mpu6050_write_reg(dev, MPU6050_REG_CONFIG, MPU6050_DLPF_CFG_FAST) != MPU6050_OK) {
        return MPU6050_ERROR_I2C;
    }
    if (mpu6050_write_reg(dev, MPU6050_REG_GYRO_CONFIG, MPU6050_GYRO_FS_250DPS) != MPU6050_OK) {
        return MPU6050_ERROR_I2C;
    }
    if (mpu6050_write_reg(dev, MPU6050_REG_ACCEL_CONFIG, MPU6050_ACCEL_FS_2G) != MPU6050_OK) {
        return MPU6050_ERROR_I2C;
    }

    dev->initialized = true;
    return MPU6050_OK;
}

mpu6050_status_t mpu6050_init_with_config(mpu6050_t *dev, const mpu6050_config_t *cfg)
{
    if ((dev == NULL) || (cfg == NULL) || (cfg->write == NULL) || (cfg->read == NULL)) {
        return MPU6050_ERROR_PARAM;
    }

    dev->write         = cfg->write;
    dev->read          = cfg->read;
    dev->delay_ms      = cfg->delay_ms;
    dev->address       = cfg->address;
    dev->skip_id_check = cfg->skip_id_check;
    dev->initialized   = false;

    return mpu6050_init_core(dev);
}

mpu6050_status_t mpu6050_init(mpu6050_t *dev,
                              uint8_t address,
                              mpu6050_i2c_write_t write,
                              mpu6050_i2c_read_t read,
                              mpu6050_delay_ms_t delay_ms)
{
    mpu6050_config_t cfg = {0};

    cfg.write    = write;
    cfg.read     = read;
    cfg.delay_ms = delay_ms;
    cfg.address  = address;
    return mpu6050_init_with_config(dev, &cfg);
}

mpu6050_status_t mpu6050_read_raw(mpu6050_t *dev,
                                  int16_t *ax,
                                  int16_t *ay,
                                  int16_t *az,
                                  int16_t *gx,
                                  int16_t *gy,
                                  int16_t *gz)
{
    uint8_t buf[14];

    if ((dev == NULL) || !dev->initialized) {
        return MPU6050_ERROR_NOT_INIT;
    }
    if ((ax == NULL) || (ay == NULL) || (az == NULL) || (gx == NULL) || (gy == NULL) || (gz == NULL)) {
        return MPU6050_ERROR_PARAM;
    }

    if (mpu6050_read_regs(dev, MPU6050_REG_ACCEL_XOUT_H, buf, sizeof(buf)) != MPU6050_OK) {
        return MPU6050_ERROR_I2C;
    }

    *ax = (int16_t)(((uint16_t)buf[0] << 8) | buf[1]);
    *ay = (int16_t)(((uint16_t)buf[2] << 8) | buf[3]);
    *az = (int16_t)(((uint16_t)buf[4] << 8) | buf[5]);
    *gx = (int16_t)(((uint16_t)buf[8] << 8) | buf[9]);
    *gy = (int16_t)(((uint16_t)buf[10] << 8) | buf[11]);
    *gz = (int16_t)(((uint16_t)buf[12] << 8) | buf[13]);
    return MPU6050_OK;
}

mpu6050_status_t mpu6050_read_temperature(mpu6050_t *dev, float *temp_c)
{
    uint8_t buf[2];
    int16_t raw;

    if ((dev == NULL) || !dev->initialized || (temp_c == NULL)) {
        return MPU6050_ERROR_NOT_INIT;
    }

    if (mpu6050_read_regs(dev, (uint8_t)(MPU6050_REG_ACCEL_XOUT_H + 6u), buf, 2u) != MPU6050_OK) {
        return MPU6050_ERROR_I2C;
    }

    raw     = (int16_t)(((uint16_t)buf[0] << 8) | buf[1]);
    *temp_c = ((float)raw / 340.0f) + 36.53f;
    return MPU6050_OK;
}
