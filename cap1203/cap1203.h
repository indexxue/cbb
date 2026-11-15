/**
 * @file    cap1203.h
 * @brief   Microchip CAP1203 电容触摸（I2C，无 MCU 依赖）。
 *
 * 通过注入 I2C write / write_read 回调绑定 HAL 或其它总线。
 * 7 位从机地址默认 0x28；寄存器为 8-bit 地址 + 8-bit 数据。
 *
 * @note    init 仅校验 Product ID，不改通道 / 灵敏度。
 */

#ifndef CAP1203_H
#define CAP1203_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CAP1203_HAL_ADDR_FROM_7BIT(addr7) ((uint8_t)((uint8_t)(addr7) << 1))

/** 默认 7 位 I2C 地址 */
#define CAP1203_ADDR_7BIT_DEFAULT (0x28u)

/** Product ID 期望值（datasheet） */
#define CAP1203_PRODUCT_ID_VALUE (0x6Du)

/** Sensor Input Status / Enable 位 */
#define CAP1203_CS1 (1u << 0)
#define CAP1203_CS2 (1u << 1)
#define CAP1203_CS3 (1u << 2)
#define CAP1203_CS_ALL (CAP1203_CS1 | CAP1203_CS2 | CAP1203_CS3)

typedef int (*cap1203_i2c_write_t)(uint8_t addr7, const uint8_t *data, uint16_t len);
typedef int (*cap1203_i2c_write_read_t)(uint8_t addr7,
					const uint8_t *tx,
					uint16_t tx_len,
					uint8_t *rx,
					uint16_t rx_len);
typedef void (*cap1203_delay_ms_t)(uint32_t ms);

typedef enum {
	CAP1203_OK = 0,
	CAP1203_ERROR_PARAM,
	CAP1203_ERROR_NOT_INIT,
	CAP1203_ERROR_I2C,
	CAP1203_ERROR_ID,
	CAP1203_ERROR_RANGE
} cap1203_status_t;

typedef struct {
	cap1203_i2c_write_t	 write;
	cap1203_i2c_write_read_t write_read;
	cap1203_delay_ms_t	 delay_ms; /* optional */
	uint8_t			 i2c_addr; /* 0 → default 0x28 */
} cap1203_config_t;

typedef struct {
	cap1203_i2c_write_t	 write;
	cap1203_i2c_write_read_t write_read;
	cap1203_delay_ms_t	 delay_ms;
	uint8_t			 i2c_addr;
	bool			 initialized;
} cap1203_t;

/** 板级 / 应用配置（显式 apply，不在 init 内偷偷写入） */
typedef struct {
	uint8_t sensor_enable;	  /* CAP1203_CSx 掩码，如 CS1|CS2 */
	uint8_t interrupt_enable; /* 通常与 sensor_enable 相同 */
	uint8_t delta_sense;	  /* 0=最灵 … 7=最钝；>7 → RANGE */
	bool	disable_multitouch_block; /* true：允许多键同时报 */
} cap1203_setup_t;

cap1203_status_t cap1203_init(cap1203_t *dev, const cap1203_config_t *cfg);
cap1203_status_t cap1203_reg_write(cap1203_t *dev, uint8_t addr, uint8_t val);
cap1203_status_t cap1203_reg_read(cap1203_t *dev, uint8_t addr, uint8_t *val);

cap1203_status_t cap1203_configure(cap1203_t *dev, const cap1203_setup_t *setup);
cap1203_status_t cap1203_read_touched(cap1203_t *dev, uint8_t *mask);
cap1203_status_t cap1203_clear_int(cap1203_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* CAP1203_H */
