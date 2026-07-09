/**
 * @file    qmc5883p.h
 * @brief   QMC5883P 三轴磁力计（I2C 回调注入，无 MCU 依赖）
 */

#ifndef QMC5883P_H
#define QMC5883P_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*qmc5883p_i2c_write_t)(uint8_t addr, const uint8_t *data, uint16_t len);
typedef int (*qmc5883p_i2c_read_t)(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len);
typedef void (*qmc5883p_delay_ms_t)(uint32_t ms);

typedef enum {
    QMC5883P_OK = 0,
    QMC5883P_ERROR_I2C,
    QMC5883P_ERROR_NOT_INIT,
    QMC5883P_ERROR_PARAM,
    QMC5883P_ERROR_ID
} qmc5883p_status_t;

typedef struct {
    qmc5883p_i2c_write_t write;
    qmc5883p_i2c_read_t  read;
    qmc5883p_delay_ms_t    delay_ms;
    uint8_t                address;
    bool                   skip_id_check;
} qmc5883p_config_t;

typedef struct {
    qmc5883p_i2c_write_t write;
    qmc5883p_i2c_read_t  read;
    qmc5883p_delay_ms_t  delay_ms;
    uint8_t              address;
    bool                 skip_id_check;
    bool                 initialized;
} qmc5883p_t;

qmc5883p_status_t qmc5883p_init_with_config(qmc5883p_t *dev, const qmc5883p_config_t *cfg);

qmc5883p_status_t qmc5883p_init(qmc5883p_t *dev,
                                  uint8_t address,
                                  qmc5883p_i2c_write_t write,
                                  qmc5883p_i2c_read_t read,
                                  qmc5883p_delay_ms_t delay_ms);

qmc5883p_status_t qmc5883p_read_raw(qmc5883p_t *dev, int16_t *mx, int16_t *my, int16_t *mz);

#ifdef __cplusplus
}
#endif

#endif /* QMC5883P_H */
