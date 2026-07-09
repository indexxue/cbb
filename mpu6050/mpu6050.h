/**
 * @file    mpu6050.h
 * @brief   MPU6050 六轴 IMU（I2C 回调注入，无 MCU 依赖）
 */

#ifndef MPU6050_H
#define MPU6050_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*mpu6050_i2c_write_t)(uint8_t addr, const uint8_t *data, uint16_t len);
typedef int (*mpu6050_i2c_read_t)(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len);
typedef void (*mpu6050_delay_ms_t)(uint32_t ms);

typedef enum {
    MPU6050_OK = 0,
    MPU6050_ERROR_I2C,
    MPU6050_ERROR_NOT_INIT,
    MPU6050_ERROR_PARAM,
    MPU6050_ERROR_ID
} mpu6050_status_t;

typedef struct {
    mpu6050_i2c_write_t write;
    mpu6050_i2c_read_t  read;
    mpu6050_delay_ms_t  delay_ms;
    uint8_t             address;
    bool                skip_id_check;
} mpu6050_config_t;

typedef struct {
    mpu6050_i2c_write_t write;
    mpu6050_i2c_read_t  read;
    mpu6050_delay_ms_t  delay_ms;
    uint8_t             address;
    bool                skip_id_check;
    bool                initialized;
} mpu6050_t;

mpu6050_status_t mpu6050_init_with_config(mpu6050_t *dev, const mpu6050_config_t *cfg);

mpu6050_status_t mpu6050_init(mpu6050_t *dev,
                              uint8_t address,
                              mpu6050_i2c_write_t write,
                              mpu6050_i2c_read_t read,
                              mpu6050_delay_ms_t delay_ms);

mpu6050_status_t mpu6050_read_raw(mpu6050_t *dev,
                                  int16_t *ax,
                                  int16_t *ay,
                                  int16_t *az,
                                  int16_t *gx,
                                  int16_t *gy,
                                  int16_t *gz);

mpu6050_status_t mpu6050_read_temperature(mpu6050_t *dev, float *temp_c);

#ifdef __cplusplus
}
#endif

#endif /* MPU6050_H */
