/**
 * @file    npm1300.c
 * @brief   nPM1300 I2C PMIC 驱动实现（A 范围）。
 */

#include "npm1300.h"

#include <stddef.h>

/* Bank bases (nPM1300 PS / ADK map — hand-written, not copied ADK) */
#define NPM1300_REG_BUCKSTATUS           0x0434u
#define NPM1300_REG_BUCK1ENASET          0x0400u
#define NPM1300_REG_BUCK1ENACLR          0x0401u
#define NPM1300_REG_BUCK2ENASET          0x0402u
#define NPM1300_REG_BUCK2ENACLR          0x0403u
#define NPM1300_REG_BUCK1NORMVOUT        0x0408u
#define NPM1300_REG_BUCK2NORMVOUT        0x040Au
#define NPM1300_REG_BUCKSWCTRLSEL        0x040Fu
#define NPM1300_REG_LDSW1SET             0x0800u
#define NPM1300_REG_LDSW1CLR             0x0801u
#define NPM1300_REG_LDSW2SET             0x0802u
#define NPM1300_REG_LDSW2CLR             0x0803u
#define NPM1300_REG_LDSWSTATUS           0x0804u
#define NPM1300_REG_LDSW1LDOSEL          0x0808u
#define NPM1300_REG_LDSW2LDOSEL          0x0809u
#define NPM1300_REG_LDSW1VOUTSEL         0x080Cu
#define NPM1300_REG_LDSW2VOUTSEL         0x080Du
#define NPM1300_REG_BCHGENABLESET        0x0304u
#define NPM1300_REG_BCHGENABLECLR        0x0305u
#define NPM1300_REG_BCHGISETMSB          0x0308u
#define NPM1300_REG_BCHGISETLSB          0x0309u
#define NPM1300_REG_BCHGVTERM            0x030Cu
#define NPM1300_REG_BCHGCHARGESTATUS     0x0334u
#define NPM1300_REG_TASKENTERSHIPMODE    0x0B02u
#define NPM1300_REG_EVENTSVBUSIN0SET     0x0016u
#define NPM1300_REG_EVENTSVBUSIN0CLR     0x0017u

#define NPM1300_TASK_PULSE               0x01u
#define NPM1300_BUCK_SWCTRL_BUCK1        0x01u
#define NPM1300_BUCK_SWCTRL_BUCK2        0x02u

static npm1300_status_t npm1300_check_bus(const npm1300_t *dev)
{
    if (dev == NULL || !dev->initialized) {
        return NPM1300_ERROR_NOT_INIT;
    }
    if (dev->write == NULL || dev->write_read == NULL) {
        return NPM1300_ERROR_PARAM;
    }
    return NPM1300_OK;
}

npm1300_status_t npm1300_reg_write(npm1300_t *dev, uint16_t addr, uint8_t val)
{
    uint8_t frame[3];
    npm1300_status_t st = npm1300_check_bus(dev);
    if (st != NPM1300_OK) {
        return st;
    }

    frame[0] = (uint8_t)(addr >> 8);
    frame[1] = (uint8_t)(addr & 0xFFu);
    frame[2] = val;
    if (dev->write(dev->i2c_addr, frame, 3u) != 0) {
        return NPM1300_ERROR_I2C;
    }
    return NPM1300_OK;
}

npm1300_status_t npm1300_reg_read(npm1300_t *dev, uint16_t addr, uint8_t *val)
{
    uint8_t tx[2];
    npm1300_status_t st = npm1300_check_bus(dev);
    if (st != NPM1300_OK) {
        return st;
    }
    if (val == NULL) {
        return NPM1300_ERROR_PARAM;
    }

    tx[0] = (uint8_t)(addr >> 8);
    tx[1] = (uint8_t)(addr & 0xFFu);
    if (dev->write_read(dev->i2c_addr, tx, 2u, val, 1u) != 0) {
        return NPM1300_ERROR_I2C;
    }
    return NPM1300_OK;
}

npm1300_status_t npm1300_init(npm1300_t *dev, const npm1300_config_t *cfg)
{
    uint8_t probe = 0u;
    uint8_t tx[2];

    if (dev == NULL || cfg == NULL) {
        return NPM1300_ERROR_PARAM;
    }
    if (cfg->write == NULL || cfg->write_read == NULL) {
        return NPM1300_ERROR_PARAM;
    }

    dev->initialized = false;
    dev->write = cfg->write;
    dev->write_read = cfg->write_read;
    dev->delay_ms = cfg->delay_ms;
    dev->i2c_addr = (cfg->i2c_addr == 0u) ? NPM1300_ADDR_7BIT_DEFAULT : cfg->i2c_addr;

    /* Probe-only: read BUCKSTATUS. Do not write any rail/charger/ship regs. */
    tx[0] = (uint8_t)(NPM1300_REG_BUCKSTATUS >> 8);
    tx[1] = (uint8_t)(NPM1300_REG_BUCKSTATUS & 0xFFu);
    if (cfg->write_read(dev->i2c_addr, tx, 2u, &probe, 1u) != 0) {
        return NPM1300_ERROR_I2C;
    }

    (void)probe;
    dev->initialized = true;
    return NPM1300_OK;
}

/* ---- stubs filled in later tasks ---- */

npm1300_status_t npm1300_buck_set_vout_mv(npm1300_t *dev, npm1300_buck_t buck, uint16_t mv)
{
    (void)dev;
    (void)buck;
    (void)mv;
    return NPM1300_ERROR_UNSUPPORTED;
}

npm1300_status_t npm1300_buck_enable(npm1300_t *dev, npm1300_buck_t buck, bool on)
{
    (void)dev;
    (void)buck;
    (void)on;
    return NPM1300_ERROR_UNSUPPORTED;
}

npm1300_status_t npm1300_buck_get_status(npm1300_t *dev, npm1300_buck_t buck, uint8_t *status)
{
    (void)dev;
    (void)buck;
    (void)status;
    return NPM1300_ERROR_UNSUPPORTED;
}

npm1300_status_t npm1300_ldsw_set_mode(npm1300_t *dev, npm1300_ldsw_t idx, npm1300_ldsw_mode_t mode)
{
    (void)dev;
    (void)idx;
    (void)mode;
    return NPM1300_ERROR_UNSUPPORTED;
}

npm1300_status_t npm1300_ldsw_set_vout_mv(npm1300_t *dev, npm1300_ldsw_t idx, uint16_t mv)
{
    (void)dev;
    (void)idx;
    (void)mv;
    return NPM1300_ERROR_UNSUPPORTED;
}

npm1300_status_t npm1300_ldsw_enable(npm1300_t *dev, npm1300_ldsw_t idx, bool on)
{
    (void)dev;
    (void)idx;
    (void)on;
    return NPM1300_ERROR_UNSUPPORTED;
}

npm1300_status_t npm1300_ldsw_get_status(npm1300_t *dev, npm1300_ldsw_t idx, uint8_t *status)
{
    (void)dev;
    (void)idx;
    (void)status;
    return NPM1300_ERROR_UNSUPPORTED;
}

npm1300_status_t npm1300_charger_set_ichg_ma(npm1300_t *dev, uint16_t ma)
{
    (void)dev;
    (void)ma;
    return NPM1300_ERROR_UNSUPPORTED;
}

npm1300_status_t npm1300_charger_set_vterm_mv(npm1300_t *dev, uint16_t mv)
{
    (void)dev;
    (void)mv;
    return NPM1300_ERROR_UNSUPPORTED;
}

npm1300_status_t npm1300_charger_enable(npm1300_t *dev, bool on)
{
    (void)dev;
    (void)on;
    return NPM1300_ERROR_UNSUPPORTED;
}

npm1300_status_t npm1300_charger_get_status(npm1300_t *dev, uint8_t *status)
{
    (void)dev;
    (void)status;
    return NPM1300_ERROR_UNSUPPORTED;
}

npm1300_status_t npm1300_ship_enter(npm1300_t *dev)
{
    (void)dev;
    return NPM1300_ERROR_UNSUPPORTED;
}

npm1300_status_t npm1300_event_read(npm1300_t *dev, uint8_t *mask)
{
    (void)dev;
    (void)mask;
    return NPM1300_ERROR_UNSUPPORTED;
}

npm1300_status_t npm1300_event_clear(npm1300_t *dev, uint8_t mask)
{
    (void)dev;
    (void)mask;
    return NPM1300_ERROR_UNSUPPORTED;
}
