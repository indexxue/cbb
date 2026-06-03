/**
 * @file    qmi8658a.h
 * @brief   Generic QMI8658A / QMI8658 6-axis IMU (I2C or SPI + delay callbacks, no MCU dependencies).
 *
 * I2C: 7-bit 0x6A (SA0 high) or 0x6B (SA0 low). WHO_AM_I = 0x05.
 * SPI: Mode 0/3; reg byte bit7 = 1 read / 0 write (see README).
 */

#ifndef QMI8658A_H
#define QMI8658A_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QMI8658A_DEFAULT_I2C_ADDR_SA0_HIGH 0x6Au
#define QMI8658A_DEFAULT_I2C_ADDR_SA0_LOW  0x6Bu

#define QMI8658A_SPI_ADDR_READ(reg)  ((uint8_t)(((reg) & 0x7Fu) | 0x80u))
#define QMI8658A_SPI_ADDR_WRITE(reg) ((uint8_t)((reg) & 0x7Fu))

typedef int (*qmi8658a_i2c_write_t)(uint8_t addr, const uint8_t *data, uint16_t len);
typedef int (*qmi8658a_i2c_read_t)(uint8_t addr, uint8_t *data, uint16_t len);
/** Optional: write then read in one transaction (repeated start). */
typedef int (*qmi8658a_i2c_write_read_t)(uint8_t addr,
                                         const uint8_t *write_data,
                                         uint16_t write_len,
                                         uint8_t *read_data,
                                         uint16_t read_len);
/** Full-duplex SPI while CS held low (board implements CS). */
typedef int (*qmi8658a_spi_xfer_t)(const uint8_t *tx, uint8_t *rx, uint16_t len);
typedef void (*qmi8658a_delay_ms_t)(uint32_t ms);

typedef enum {
    QMI8658A_OK = 0,
    QMI8658A_ERROR_I2C,
    QMI8658A_ERROR_NOT_INIT,
    QMI8658A_ERROR_PARAM,
    QMI8658A_ERROR_ID
} qmi8658a_status_t;

typedef enum {
    QMI8658A_ACCEL_RANGE_2G  = 0u,
    QMI8658A_ACCEL_RANGE_4G  = 1u,
    QMI8658A_ACCEL_RANGE_8G  = 2u,
    QMI8658A_ACCEL_RANGE_16G = 3u
} qmi8658a_accel_range_t;

typedef enum {
    QMI8658A_GYRO_RANGE_16DPS   = 0u,
    QMI8658A_GYRO_RANGE_32DPS   = 1u,
    QMI8658A_GYRO_RANGE_64DPS   = 2u,
    QMI8658A_GYRO_RANGE_128DPS  = 3u,
    QMI8658A_GYRO_RANGE_256DPS  = 4u,
    QMI8658A_GYRO_RANGE_512DPS  = 5u,
    QMI8658A_GYRO_RANGE_1024DPS = 6u,
    QMI8658A_GYRO_RANGE_2048DPS = 7u
} qmi8658a_gyro_range_t;

typedef struct {
    qmi8658a_i2c_write_t      write;
    qmi8658a_i2c_read_t       read;
    qmi8658a_i2c_write_read_t write_read;
    qmi8658a_spi_xfer_t       spi_xfer;
    qmi8658a_delay_ms_t       delay_ms;
    uint8_t                   address;
    bool                      spi_3wire;
    qmi8658a_accel_range_t    accel_range;
    qmi8658a_gyro_range_t     gyro_range;
} qmi8658a_config_t;

typedef struct {
    qmi8658a_i2c_write_t      write;
    qmi8658a_i2c_read_t       read;
    qmi8658a_i2c_write_read_t write_read;
    qmi8658a_spi_xfer_t       spi_xfer;
    qmi8658a_delay_ms_t       delay_ms;
    uint8_t                   address;
    bool                      spi_3wire;
    qmi8658a_accel_range_t    accel_range;
    qmi8658a_gyro_range_t     gyro_range;
    bool                      initialized;
} qmi8658a_t;

qmi8658a_status_t qmi8658a_init_with_config(qmi8658a_t *dev, const qmi8658a_config_t *cfg);

qmi8658a_status_t qmi8658a_init(qmi8658a_t *dev,
                                uint8_t address,
                                qmi8658a_i2c_write_t write,
                                qmi8658a_i2c_read_t read,
                                qmi8658a_delay_ms_t delay_ms);

qmi8658a_status_t qmi8658a_init_spi(qmi8658a_t *dev,
                                   qmi8658a_spi_xfer_t xfer,
                                   qmi8658a_delay_ms_t delay_ms);

qmi8658a_status_t qmi8658a_set_accel_range(qmi8658a_t *dev, qmi8658a_accel_range_t range);
qmi8658a_status_t qmi8658a_set_gyro_range(qmi8658a_t *dev, qmi8658a_gyro_range_t range);

qmi8658a_status_t qmi8658a_read_raw(qmi8658a_t *dev,
                                    int16_t *ax,
                                    int16_t *ay,
                                    int16_t *az,
                                    int16_t *gx,
                                    int16_t *gy,
                                    int16_t *gz);

qmi8658a_status_t qmi8658a_read_temperature(qmi8658a_t *dev, float *temp_c);

#ifdef __cplusplus
}
#endif

#endif /* QMI8658A_H */
