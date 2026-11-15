/**
 * @file    npm1300.h
 * @brief   Nordic nPM1300 PMIC 通用驱动（I2C，无 MCU 依赖）。
 *
 * 通过注入 I2C write / write_read 回调绑定 HAL 或其它总线实现。
 * 7 位从机地址默认 0x6B；寄存器为 16-bit 大端地址 + 8-bit 数据。
 *
 * @note    init 仅做通信探测，不改电源轨 / 充电 / Ship。
 */

#ifndef NPM1300_H
#define NPM1300_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** STM32 HAL_I2C_* 的 DevAddress：7 位地址左移 1 位 */
#define NPM1300_HAL_ADDR_FROM_7BIT(addr7) ((uint8_t)((uint8_t)(addr7) << 1))

/** 默认 7 位 I2C 地址 */
#define NPM1300_ADDR_7BIT_DEFAULT (0x6Bu)

typedef int (*npm1300_i2c_write_t)(uint8_t addr7, const uint8_t *data, uint16_t len);
typedef int (*npm1300_i2c_write_read_t)(uint8_t addr7,
                                        const uint8_t *tx,
                                        uint16_t tx_len,
                                        uint8_t *rx,
                                        uint16_t rx_len);
typedef void (*npm1300_delay_ms_t)(uint32_t ms);

typedef enum {
    NPM1300_OK = 0,
    NPM1300_ERROR_PARAM,
    NPM1300_ERROR_NOT_INIT,
    NPM1300_ERROR_I2C,
    NPM1300_ERROR_RANGE,
    NPM1300_ERROR_UNSUPPORTED
} npm1300_status_t;

typedef enum {
    NPM1300_BUCK1 = 1,
    NPM1300_BUCK2 = 2
} npm1300_buck_t;

typedef enum {
    NPM1300_LDSW1 = 1,
    NPM1300_LDSW2 = 2
} npm1300_ldsw_t;

typedef enum {
    NPM1300_LDSW_MODE_LOADSW = 0,
    NPM1300_LDSW_MODE_LDO = 1
} npm1300_ldsw_mode_t;

typedef struct {
    npm1300_i2c_write_t      write;
    npm1300_i2c_write_read_t write_read;
    npm1300_delay_ms_t       delay_ms; /* optional */
    uint8_t                  i2c_addr; /* 0 → default 0x6B */
} npm1300_config_t;

typedef struct {
    npm1300_i2c_write_t      write;
    npm1300_i2c_write_read_t write_read;
    npm1300_delay_ms_t       delay_ms;
    uint8_t                  i2c_addr;
    bool                     initialized;
} npm1300_t;

npm1300_status_t npm1300_init(npm1300_t *dev, const npm1300_config_t *cfg);
npm1300_status_t npm1300_reg_write(npm1300_t *dev, uint16_t addr, uint8_t val);
npm1300_status_t npm1300_reg_read(npm1300_t *dev, uint16_t addr, uint8_t *val);

npm1300_status_t npm1300_buck_set_vout_mv(npm1300_t *dev, npm1300_buck_t buck, uint16_t mv);
npm1300_status_t npm1300_buck_enable(npm1300_t *dev, npm1300_buck_t buck, bool on);
npm1300_status_t npm1300_buck_get_status(npm1300_t *dev, npm1300_buck_t buck, uint8_t *status);

npm1300_status_t npm1300_ldsw_set_mode(npm1300_t *dev, npm1300_ldsw_t idx, npm1300_ldsw_mode_t mode);
npm1300_status_t npm1300_ldsw_set_vout_mv(npm1300_t *dev, npm1300_ldsw_t idx, uint16_t mv);
npm1300_status_t npm1300_ldsw_enable(npm1300_t *dev, npm1300_ldsw_t idx, bool on);
npm1300_status_t npm1300_ldsw_get_status(npm1300_t *dev, npm1300_ldsw_t idx, uint8_t *status);

npm1300_status_t npm1300_charger_set_ichg_ma(npm1300_t *dev, uint16_t ma);
npm1300_status_t npm1300_charger_set_vterm_mv(npm1300_t *dev, uint16_t mv);
npm1300_status_t npm1300_charger_enable(npm1300_t *dev, bool on);
npm1300_status_t npm1300_charger_get_status(npm1300_t *dev, uint8_t *status);

npm1300_status_t npm1300_ship_enter(npm1300_t *dev);
npm1300_status_t npm1300_event_read(npm1300_t *dev, uint8_t *mask);
npm1300_status_t npm1300_event_clear(npm1300_t *dev, uint8_t mask);

#ifdef __cplusplus
}
#endif

#endif /* NPM1300_H */
