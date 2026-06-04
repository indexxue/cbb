/**
 * @file    qmi8658a.h
 * @brief   Generic QMI8658A / QMI8658 6-axis IMU (I2C or SPI + delay callbacks, no MCU dependencies).
 *
 * I2C: typical 7-bit address 0x6A (SA0 high) or 0x6B (SA0 low). WHO_AM_I = 0x05.
 * SPI: CS 低选中；地址字节 bit7=1 读 / bit7=0 写（见 QMI8658A_SPI_ADDR_*）。硬件 CS=0 进入 SPI 模式。
 */

#ifndef QMI8658A_H
#define QMI8658A_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** SPI 地址字节：bit7=1 读，bit7=0 写（与数据手册 Figure 28 / SensorLib 一致）。 */
#define QMI8658A_SPI_READ_FLAG 0x80u
#define QMI8658A_SPI_ADDR_WRITE(reg) ((uint8_t)(reg))
#define QMI8658A_SPI_ADDR_READ(reg)  ((uint8_t)((uint8_t)(reg) | QMI8658A_SPI_READ_FLAG))

typedef int (*qmi8658a_i2c_write_t)(uint8_t addr, const uint8_t *data, uint16_t len);
typedef int (*qmi8658a_i2c_read_t)(uint8_t addr, uint8_t *data, uint16_t len);
/** 可选：一次事务内先写再读（如 I2C repeated start）。非 NULL 时读寄存器优先走此路径。 */
typedef int (*qmi8658a_i2c_write_read_t)(uint8_t addr,
                                       const uint8_t *write_data,
                                       uint16_t write_len,
                                       uint8_t *read_data,
                                       uint16_t read_len);
typedef int (*qmi8658a_spi_tx_t)(const uint8_t *tx, uint16_t len);
/** SPI 读寄存器必需：全双工收发，首字节 TX 为 reg|0x80，RX[1…] 为数据。 */
typedef int (*qmi8658a_spi_tx_rx_t)(const uint8_t *tx, uint8_t *rx, uint16_t len);
/** 片选：0=CS 有效（低），1=释放。 */
typedef void (*qmi8658a_set_cs_t)(uint8_t level);
typedef void (*qmi8658a_delay_ms_t)(uint32_t ms);

typedef enum {
    QMI8658A_OK = 0,
    QMI8658A_ERROR_I2C,
    QMI8658A_ERROR_SPI,
    QMI8658A_ERROR_NOT_INIT,
    QMI8658A_ERROR_PARAM,
    QMI8658A_ERROR_ID
} qmi8658a_status_t;

typedef enum {
    QMI8658A_BUS_I2C = 0,
    QMI8658A_BUS_SPI = 1
} qmi8658a_bus_t;

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
    qmi8658a_i2c_write_t        write;
    qmi8658a_i2c_read_t         read;
    qmi8658a_i2c_write_read_t   write_read;
    qmi8658a_delay_ms_t         delay_ms;
    uint8_t                     address;
    qmi8658a_accel_range_t      accel_range;
    qmi8658a_gyro_range_t       gyro_range;
} qmi8658a_config_t;

typedef struct {
    qmi8658a_spi_tx_t           spi_tx;
    qmi8658a_spi_tx_rx_t        spi_tx_rx;
    qmi8658a_set_cs_t           set_cs;
    qmi8658a_delay_ms_t         delay_ms;
    qmi8658a_accel_range_t      accel_range;
    qmi8658a_gyro_range_t       gyro_range;
    /** false=4 线 SPI（默认）；true=3 线 SPI（CTRL1.SIM=1）。 */
    bool                        spi_3wire;
} qmi8658a_spi_config_t;

typedef struct {
    qmi8658a_bus_t              bus;
    qmi8658a_i2c_write_t          write;
    qmi8658a_i2c_read_t           read;
    qmi8658a_i2c_write_read_t     write_read;
    qmi8658a_spi_tx_t             spi_tx;
    qmi8658a_spi_tx_rx_t          spi_tx_rx;
    qmi8658a_set_cs_t             set_cs;
    qmi8658a_delay_ms_t           delay_ms;
    uint8_t                       address;
    qmi8658a_accel_range_t        accel_range;
    qmi8658a_gyro_range_t         gyro_range;
    bool                          spi_3wire;
    bool                          initialized;
} qmi8658a_t;

qmi8658a_status_t qmi8658a_init_with_config(qmi8658a_t *dev, const qmi8658a_config_t *cfg);

qmi8658a_status_t qmi8658a_init(qmi8658a_t *dev,
                                uint8_t address,
                                qmi8658a_i2c_write_t write,
                                qmi8658a_i2c_read_t read,
                                qmi8658a_delay_ms_t delay_ms);

qmi8658a_status_t qmi8658a_init_spi_with_config(qmi8658a_t *dev, const qmi8658a_spi_config_t *cfg);

qmi8658a_status_t qmi8658a_init_spi(qmi8658a_t *dev,
                                    qmi8658a_spi_tx_t spi_tx,
                                    qmi8658a_spi_tx_rx_t spi_tx_rx,
                                    qmi8658a_set_cs_t set_cs,
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
