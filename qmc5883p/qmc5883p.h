/**
 * @file    qmc5883p.h
 * @brief   Generic QMC5883P 3-axis magnetometer (I2C + delay callbacks, no MCU dependencies).
 *
 * Typical 7-bit I2C address: 0x2C. CHIPID (register 0x00) = 0x80.
 */

#ifndef QMC5883P_H
#define QMC5883P_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Default 7-bit I2C slave address. */
#define QMC5883P_DEFAULT_I2C_ADDR 0x2Cu

typedef int (*qmc5883p_i2c_write_t)(uint8_t addr, const uint8_t *data, uint16_t len);
typedef int (*qmc5883p_i2c_read_t)(uint8_t addr, uint8_t *data, uint16_t len);
/** Optional: write then read in one transaction (repeated start). */
typedef int (*qmc5883p_i2c_write_read_t)(uint8_t addr,
                                         const uint8_t *write_data,
                                         uint16_t write_len,
                                         uint8_t *read_data,
                                         uint16_t read_len);
typedef void (*qmc5883p_delay_ms_t)(uint32_t ms);

typedef enum {
    QMC5883P_OK = 0,
    QMC5883P_ERROR_I2C,
    QMC5883P_ERROR_NOT_INIT,
    QMC5883P_ERROR_PARAM,
    QMC5883P_ERROR_ID
} qmc5883p_status_t;

typedef enum {
    QMC5883P_MODE_SUSPEND    = 0u,
    QMC5883P_MODE_NORMAL     = 1u,
    QMC5883P_MODE_SINGLE     = 2u,
    QMC5883P_MODE_CONTINUOUS = 3u
} qmc5883p_mode_t;

typedef enum {
    QMC5883P_ODR_10HZ  = 0u,
    QMC5883P_ODR_50HZ  = 1u,
    QMC5883P_ODR_100HZ = 2u,
    QMC5883P_ODR_200HZ = 3u
} qmc5883p_odr_t;

/** OSR1 (control register 1, bits 4:5). */
typedef enum {
    QMC5883P_OSR_8 = 0u,
    QMC5883P_OSR_4 = 1u,
    QMC5883P_OSR_2 = 2u,
    QMC5883P_OSR_1 = 3u
} qmc5883p_osr_t;

/** OSR2 / down-sample (control register 1, bits 6:7). */
typedef enum {
    QMC5883P_DSR_1 = 0u,
    QMC5883P_DSR_2 = 1u,
    QMC5883P_DSR_4 = 2u,
    QMC5883P_DSR_8 = 3u
} qmc5883p_dsr_t;

typedef enum {
    QMC5883P_RANGE_30G = 0u,
    QMC5883P_RANGE_12G = 1u,
    QMC5883P_RANGE_8G  = 2u,
    QMC5883P_RANGE_2G  = 3u
} qmc5883p_range_t;

typedef enum {
    QMC5883P_SETRESET_ON      = 0u,
    QMC5883P_SETRESET_SETONLY = 1u,
    QMC5883P_SETRESET_OFF     = 2u
} qmc5883p_setreset_t;

typedef struct {
    qmc5883p_i2c_write_t      write;
    qmc5883p_i2c_read_t       read;
    qmc5883p_i2c_write_read_t write_read;
    qmc5883p_delay_ms_t       delay_ms;
    uint8_t                   address;
    qmc5883p_mode_t           mode;
    qmc5883p_odr_t            odr;
    qmc5883p_osr_t            osr;
    qmc5883p_dsr_t            dsr;
    qmc5883p_range_t          range;
    qmc5883p_setreset_t       setreset;
} qmc5883p_config_t;

typedef struct {
    qmc5883p_i2c_write_t      write;
    qmc5883p_i2c_read_t       read;
    qmc5883p_i2c_write_read_t write_read;
    qmc5883p_delay_ms_t       delay_ms;
    uint8_t                   address;
    qmc5883p_mode_t           mode;
    qmc5883p_odr_t            odr;
    qmc5883p_osr_t            osr;
    qmc5883p_dsr_t            dsr;
    qmc5883p_range_t          range;
    qmc5883p_setreset_t       setreset;
    bool                      initialized;
} qmc5883p_t;

qmc5883p_status_t qmc5883p_init_with_config(qmc5883p_t *dev, const qmc5883p_config_t *cfg);

qmc5883p_status_t qmc5883p_init(qmc5883p_t *dev,
                                qmc5883p_i2c_write_t write,
                                qmc5883p_i2c_read_t read,
                                qmc5883p_delay_ms_t delay_ms);

qmc5883p_status_t qmc5883p_soft_reset(qmc5883p_t *dev);

qmc5883p_status_t qmc5883p_set_mode(qmc5883p_t *dev, qmc5883p_mode_t mode);
qmc5883p_status_t qmc5883p_set_odr(qmc5883p_t *dev, qmc5883p_odr_t odr);
qmc5883p_status_t qmc5883p_set_range(qmc5883p_t *dev, qmc5883p_range_t range);

qmc5883p_status_t qmc5883p_read_raw(qmc5883p_t *dev, int16_t *mx, int16_t *my, int16_t *mz);

qmc5883p_status_t qmc5883p_read_gauss(qmc5883p_t *dev, float *mx, float *my, float *mz);

qmc5883p_status_t qmc5883p_data_ready(qmc5883p_t *dev, bool *ready);

qmc5883p_status_t qmc5883p_overflow(qmc5883p_t *dev, bool *overflow);

#ifdef __cplusplus
}
#endif

#endif /* QMC5883P_H */
