/**
 * @file    ina226.h
 * @brief   INA226 高精度双向电流/功率监视器（I2C，无 MCU 依赖）。
 *
 * 通过注入 I2C 写/读/写读回调绑定 HAL 或其它总线实现。
 * 7 位从机地址默认 0x40（A1:A0 绑线可至 0x4F）。
 *
 * @see TI INA226 datasheet (SBOS547)
 */

#ifndef INA226_H
#define INA226_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** STM32 HAL_I2C_* 的 DevAddress：7 位地址左移 1 位 */
#define INA226_HAL_ADDR_FROM_7BIT(addr7) ((uint8_t)((uint8_t)(addr7) << 1))

/** 7 位 I2C 地址：0x40 | (A1<<1) | A0 */
#define INA226_ADDR_7BIT(a1, a0) ((uint8_t)(0x40u | (((uint8_t)(a1) & 1u) << 1) | ((uint8_t)(a0) & 1u)))

#define INA226_ADDR_DEFAULT INA226_ADDR_7BIT(0u, 0u)

/** 校准常数（datasheet） */
#define INA226_CALIBRATION_CONST 0.00512f

typedef int (*ina226_i2c_write_t)(uint8_t i2c_addr, const uint8_t *data, uint16_t len);
typedef int (*ina226_i2c_read_t)(uint8_t i2c_addr, uint8_t *data, uint16_t len);
typedef int (*ina226_i2c_write_read_t)(uint8_t i2c_addr,
                                       const uint8_t *tx,
                                       uint16_t tx_len,
                                       uint8_t *rx,
                                       uint16_t rx_len);

typedef enum {
    INA226_OK = 0,
    INA226_ERROR_PARAM,
    INA226_ERROR_NOT_INIT,
    INA226_ERROR_I2C,
    INA226_ERROR_RANGE,
    INA226_ERROR_CALIB
} ina226_status_t;

/** 寄存器地址 */
#define INA226_REG_CONFIG       0x00u
#define INA226_REG_SHUNT_VOLT   0x01u
#define INA226_REG_BUS_VOLT     0x02u
#define INA226_REG_POWER        0x03u
#define INA226_REG_CURRENT      0x04u
#define INA226_REG_CALIBRATION  0x05u
#define INA226_REG_MASK_ENABLE  0x06u
#define INA226_REG_ALERT_LIMIT  0x07u

/** Configuration：RST */
#define INA226_CFG_RST (1u << 15)

/** Configuration：BRNG — 0=16V 总线量程，1=80V */
typedef enum {
    INA226_BUS_RANGE_16V = 0u,
    INA226_BUS_RANGE_80V = 1u
} ina226_bus_range_t;

/** Configuration：AVG[2:0] */
typedef enum {
    INA226_AVG_1   = 0u,
    INA226_AVG_4   = 1u,
    INA226_AVG_16  = 2u,
    INA226_AVG_64  = 3u,
    INA226_AVG_128 = 4u,
    INA226_AVG_256 = 5u,
    INA226_AVG_512 = 6u,
    INA226_AVG_1024 = 7u
} ina226_avg_t;

/** Configuration：VBUSCT / VSHCT 转换时间 */
typedef enum {
    INA226_CT_140US  = 0u,
    INA226_CT_204US  = 1u,
    INA226_CT_332US  = 2u,
    INA226_CT_588US  = 3u,
    INA226_CT_1100US = 4u,
    INA226_CT_2116US = 5u,
    INA226_CT_4156US = 6u,
    INA226_CT_8244US = 7u
} ina226_conv_time_t;

/** Configuration：MODE[2:0] */
typedef enum {
    INA226_MODE_POWER_DOWN            = 0u,
    INA226_MODE_SHUNT_TRIG             = 1u,
    INA226_MODE_BUS_TRIG               = 2u,
    INA226_MODE_SHUNT_BUS_TRIG         = 3u,
    INA226_MODE_POWER_DOWN2            = 4u,
    INA226_MODE_SHUNT_CONT             = 5u,
    INA226_MODE_BUS_CONT               = 6u,
    INA226_MODE_SHUNT_BUS_CONT         = 7u
} ina226_mode_t;

typedef struct {
    ina226_bus_range_t bus_range;
    ina226_avg_t       avg;
    ina226_conv_time_t vbus_ct;
    ina226_conv_time_t vshunt_ct;
    ina226_mode_t      mode;
} ina226_measure_cfg_t;

/** 常用默认：16V 总线、16 次平均、1.1ms 转换、连续分流+总线 */
#define INA226_MEASURE_CFG_DEFAULT                 \
    {                                              \
        .bus_range = INA226_BUS_RANGE_16V,         \
        .avg       = INA226_AVG_16,                \
        .vbus_ct   = INA226_CT_1100US,             \
        .vshunt_ct = INA226_CT_1100US,             \
        .mode      = INA226_MODE_SHUNT_BUS_CONT,   \
    }

typedef struct {
    ina226_i2c_write_t      write;
    ina226_i2c_read_t       read;
    ina226_i2c_write_read_t write_read;
    uint8_t                 i2c_addr;
    float                   shunt_ohm;
    float                   max_expected_a;
    ina226_measure_cfg_t    measure;
} ina226_config_t;

typedef struct {
    ina226_i2c_write_t      write;
    ina226_i2c_read_t       read;
    ina226_i2c_write_read_t write_read;
    uint8_t                 i2c_addr;
    float                   shunt_ohm;
    float                   current_lsb;
    float                   power_lsb;
    uint16_t                cal_reg;
    bool                    initialized;
} ina226_t;

typedef struct {
    float shunt_v;
    float bus_v;
    float current_a;
    float power_w;
} ina226_readings_t;

ina226_status_t ina226_init(ina226_t *dev, const ina226_config_t *cfg);

/** 软件复位（Configuration.RST） */
ina226_status_t ina226_reset(ina226_t *dev);

/** 写 Configuration 寄存器（不含校准） */
ina226_status_t ina226_configure(ina226_t *dev, const ina226_measure_cfg_t *cfg);

/**
 * 根据分流电阻与期望最大电流写入 Calibration，并缓存 current_lsb / power_lsb。
 * current_lsb = max_expected_a / 32768
 */
ina226_status_t ina226_set_calibration(ina226_t *dev, float shunt_ohm, float max_expected_a);

ina226_status_t ina226_write_reg(ina226_t *dev, uint8_t reg, uint16_t value);
ina226_status_t ina226_read_reg(ina226_t *dev, uint8_t reg, uint16_t *value);

ina226_status_t ina226_read_shunt_raw(ina226_t *dev, int16_t *raw);
ina226_status_t ina226_read_bus_raw(ina226_t *dev, uint16_t *raw);
ina226_status_t ina226_read_current_raw(ina226_t *dev, int16_t *raw);
ina226_status_t ina226_read_power_raw(ina226_t *dev, uint16_t *raw);

ina226_status_t ina226_read_shunt_v(ina226_t *dev, float *v);
ina226_status_t ina226_read_bus_v(ina226_t *dev, float *v);
ina226_status_t ina226_read_current_a(ina226_t *dev, float *a);
ina226_status_t ina226_read_power_w(ina226_t *dev, float *w);

/** 一次读取分流/总线/电流/功率（物理量，需已校准） */
ina226_status_t ina226_read_all(ina226_t *dev, ina226_readings_t *out);

/** 由 Configuration 各字段拼 16 位配置字 */
uint16_t ina226_pack_config(const ina226_measure_cfg_t *cfg);

/** 由分流电压（V）与分流电阻估算电流（未走芯片 Current 寄存器） */
float ina226_current_from_shunt(float shunt_v, float shunt_ohm);

#ifdef __cplusplus
}
#endif

#endif /* INA226_H */
