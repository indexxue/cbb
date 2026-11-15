/**
 * @file    cap1203.c
 * @brief   CAP1203 I2C 驱动（A 范围）。
 */

#include "cap1203.h"

#include <stddef.h>

#define CAP1203_REG_MAIN_CONTROL	  0x00u
#define CAP1203_REG_GENERAL_STATUS	  0x02u
#define CAP1203_REG_SENSOR_INPUT_STATUS	  0x03u
#define CAP1203_REG_SENSITIVITY_CONTROL 0x1Fu
#define CAP1203_REG_SENSOR_INPUT_ENABLE 0x21u
#define CAP1203_REG_INTERRUPT_ENABLE	  0x27u
#define CAP1203_REG_MULTIPLE_TOUCH_CFG  0x2Au
#define CAP1203_REG_PRODUCT_ID		  0xFDu

#define CAP1203_MAIN_INT		  0x01u
#define CAP1203_MULT_BLK_EN		  0x80u
#define CAP1203_SENS_DEFAULT_BASE_SHIFT 0x0Fu

static cap1203_status_t cap1203_check_bus(const cap1203_t *dev)
{
	if (dev == NULL || !dev->initialized) {
		return CAP1203_ERROR_NOT_INIT;
	}
	if (dev->write == NULL || dev->write_read == NULL) {
		return CAP1203_ERROR_PARAM;
	}
	return CAP1203_OK;
}

cap1203_status_t cap1203_reg_write(cap1203_t *dev, uint8_t addr, uint8_t val)
{
	uint8_t frame[2];
	cap1203_status_t st = cap1203_check_bus(dev);

	if (st != CAP1203_OK) {
		return st;
	}

	frame[0] = addr;
	frame[1] = val;
	if (dev->write(dev->i2c_addr, frame, 2u) != 0) {
		return CAP1203_ERROR_I2C;
	}
	return CAP1203_OK;
}

cap1203_status_t cap1203_reg_read(cap1203_t *dev, uint8_t addr, uint8_t *val)
{
	cap1203_status_t st = cap1203_check_bus(dev);

	if (st != CAP1203_OK) {
		return st;
	}
	if (val == NULL) {
		return CAP1203_ERROR_PARAM;
	}
	if (dev->write_read(dev->i2c_addr, &addr, 1u, val, 1u) != 0) {
		return CAP1203_ERROR_I2C;
	}
	return CAP1203_OK;
}

cap1203_status_t cap1203_init(cap1203_t *dev, const cap1203_config_t *cfg)
{
	uint8_t pid = 0u;
	uint8_t reg = CAP1203_REG_PRODUCT_ID;

	if (dev == NULL || cfg == NULL) {
		return CAP1203_ERROR_PARAM;
	}
	if (cfg->write == NULL || cfg->write_read == NULL) {
		return CAP1203_ERROR_PARAM;
	}

	dev->initialized = false;
	dev->write = cfg->write;
	dev->write_read = cfg->write_read;
	dev->delay_ms = cfg->delay_ms;
	dev->i2c_addr = (cfg->i2c_addr == 0u) ? CAP1203_ADDR_7BIT_DEFAULT : cfg->i2c_addr;

	if (cfg->write_read(dev->i2c_addr, &reg, 1u, &pid, 1u) != 0) {
		return CAP1203_ERROR_I2C;
	}
	if (pid != CAP1203_PRODUCT_ID_VALUE) {
		return CAP1203_ERROR_ID;
	}

	dev->initialized = true;
	return CAP1203_OK;
}

cap1203_status_t cap1203_configure(cap1203_t *dev, const cap1203_setup_t *setup)
{
	cap1203_status_t st;
	uint8_t sens;
	uint8_t mtc;

	st = cap1203_check_bus(dev);
	if (st != CAP1203_OK) {
		return st;
	}
	if (setup == NULL) {
		return CAP1203_ERROR_PARAM;
	}
	if ((setup->sensor_enable & ~CAP1203_CS_ALL) != 0u ||
	    (setup->interrupt_enable & ~CAP1203_CS_ALL) != 0u) {
		return CAP1203_ERROR_RANGE;
	}
	if (setup->delta_sense > 7u) {
		return CAP1203_ERROR_RANGE;
	}

	st = cap1203_reg_write(dev, CAP1203_REG_SENSOR_INPUT_ENABLE, setup->sensor_enable);
	if (st != CAP1203_OK) {
		return st;
	}
	st = cap1203_reg_write(dev, CAP1203_REG_INTERRUPT_ENABLE, setup->interrupt_enable);
	if (st != CAP1203_OK) {
		return st;
	}

	/* DELTA_SENSE[6:4] + BASE_SHIFT[3:0]；保持默认 BASE_SHIFT=0xF */
	sens = (uint8_t)(((setup->delta_sense & 0x07u) << 4) | CAP1203_SENS_DEFAULT_BASE_SHIFT);
	st = cap1203_reg_write(dev, CAP1203_REG_SENSITIVITY_CONTROL, sens);
	if (st != CAP1203_OK) {
		return st;
	}

	st = cap1203_reg_read(dev, CAP1203_REG_MULTIPLE_TOUCH_CFG, &mtc);
	if (st != CAP1203_OK) {
		return st;
	}
	if (setup->disable_multitouch_block) {
		mtc = (uint8_t)(mtc & (uint8_t)~CAP1203_MULT_BLK_EN);
	} else {
		mtc = (uint8_t)(mtc | CAP1203_MULT_BLK_EN);
	}
	st = cap1203_reg_write(dev, CAP1203_REG_MULTIPLE_TOUCH_CFG, mtc);
	if (st != CAP1203_OK) {
		return st;
	}

	return cap1203_clear_int(dev);
}

cap1203_status_t cap1203_read_touched(cap1203_t *dev, uint8_t *mask)
{
	cap1203_status_t st;
	uint8_t status = 0u;

	if (mask == NULL) {
		return CAP1203_ERROR_PARAM;
	}
	st = cap1203_reg_read(dev, CAP1203_REG_SENSOR_INPUT_STATUS, &status);
	if (st != CAP1203_OK) {
		return st;
	}
	*mask = (uint8_t)(status & CAP1203_CS_ALL);
	return CAP1203_OK;
}

cap1203_status_t cap1203_clear_int(cap1203_t *dev)
{
	cap1203_status_t st;
	uint8_t main_ctl = 0u;

	st = cap1203_reg_read(dev, CAP1203_REG_MAIN_CONTROL, &main_ctl);
	if (st != CAP1203_OK) {
		return st;
	}
	main_ctl = (uint8_t)(main_ctl & (uint8_t)~CAP1203_MAIN_INT);
	return cap1203_reg_write(dev, CAP1203_REG_MAIN_CONTROL, main_ctl);
}
