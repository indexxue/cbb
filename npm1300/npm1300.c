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
#define NPM1300_REG_BUCK1VOUTSTATUS      0x0410u
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

static npm1300_status_t npm1300_mv_to_buck_code(uint16_t mv, uint8_t *code)
{
    uint16_t c;

    if (code == NULL) {
        return NPM1300_ERROR_PARAM;
    }
    if (mv < 1000u || mv > 3300u || ((mv - 1000u) % 100u) != 0u) {
        return NPM1300_ERROR_RANGE;
    }
    c = (uint16_t)((mv - 1000u) / 100u);
    *code = (uint8_t)c;
    return NPM1300_OK;
}

npm1300_status_t npm1300_buck_set_vout_mv(npm1300_t *dev, npm1300_buck_t buck, uint16_t mv)
{
    uint8_t code = 0u;
    uint8_t swctrl = 0u;
    uint16_t vout_reg;
    uint8_t sw_bit;
    npm1300_status_t st;

    st = npm1300_check_bus(dev);
    if (st != NPM1300_OK) {
        return st;
    }
    if (buck != NPM1300_BUCK1 && buck != NPM1300_BUCK2) {
        return NPM1300_ERROR_PARAM;
    }

    st = npm1300_mv_to_buck_code(mv, &code);
    if (st != NPM1300_OK) {
        return st;
    }

    if (buck == NPM1300_BUCK1) {
        vout_reg = NPM1300_REG_BUCK1NORMVOUT;
        sw_bit = NPM1300_BUCK_SWCTRL_BUCK1;
    } else {
        vout_reg = NPM1300_REG_BUCK2NORMVOUT;
        sw_bit = NPM1300_BUCK_SWCTRL_BUCK2;
    }

    /* Program NORMVOUT before enabling SWCTRL — otherwise first switch can
     * briefly apply reset VOUT (e.g. 1.2 V) and brown out the rail. */
    st = npm1300_reg_write(dev, vout_reg, code);
    if (st != NPM1300_OK) {
        return st;
    }
    st = npm1300_reg_read(dev, NPM1300_REG_BUCKSWCTRLSEL, &swctrl);
    if (st != NPM1300_OK) {
        return st;
    }
    swctrl = (uint8_t)(swctrl | sw_bit);
    return npm1300_reg_write(dev, NPM1300_REG_BUCKSWCTRLSEL, swctrl);
}

npm1300_status_t npm1300_buck_enable(npm1300_t *dev, npm1300_buck_t buck, bool on)
{
    uint16_t reg;
    npm1300_status_t st = npm1300_check_bus(dev);
    if (st != NPM1300_OK) {
        return st;
    }
    if (buck != NPM1300_BUCK1 && buck != NPM1300_BUCK2) {
        return NPM1300_ERROR_PARAM;
    }

    if (buck == NPM1300_BUCK1) {
        reg = on ? NPM1300_REG_BUCK1ENASET : NPM1300_REG_BUCK1ENACLR;
    } else {
        reg = on ? NPM1300_REG_BUCK2ENASET : NPM1300_REG_BUCK2ENACLR;
    }
    return npm1300_reg_write(dev, reg, NPM1300_TASK_PULSE);
}

npm1300_status_t npm1300_buck_get_status(npm1300_t *dev, npm1300_buck_t buck, uint8_t *status)
{
    npm1300_status_t st = npm1300_check_bus(dev);
    if (st != NPM1300_OK) {
        return st;
    }
    if (status == NULL || (buck != NPM1300_BUCK1 && buck != NPM1300_BUCK2)) {
        return NPM1300_ERROR_PARAM;
    }
    (void)buck; /* shared BUCKSTATUS register */
    return npm1300_reg_read(dev, NPM1300_REG_BUCKSTATUS, status);
}

npm1300_status_t npm1300_ldsw_set_mode(npm1300_t *dev, npm1300_ldsw_t idx, npm1300_ldsw_mode_t mode)
{
    uint16_t reg;
    uint8_t val;
    npm1300_status_t st = npm1300_check_bus(dev);
    if (st != NPM1300_OK) {
        return st;
    }
    if (idx != NPM1300_LDSW1 && idx != NPM1300_LDSW2) {
        return NPM1300_ERROR_PARAM;
    }
    if (mode != NPM1300_LDSW_MODE_LOADSW && mode != NPM1300_LDSW_MODE_LDO) {
        return NPM1300_ERROR_PARAM;
    }

    reg = (idx == NPM1300_LDSW1) ? NPM1300_REG_LDSW1LDOSEL : NPM1300_REG_LDSW2LDOSEL;
    val = (mode == NPM1300_LDSW_MODE_LDO) ? 0x01u : 0x00u;
    return npm1300_reg_write(dev, reg, val);
}

npm1300_status_t npm1300_ldsw_set_vout_mv(npm1300_t *dev, npm1300_ldsw_t idx, uint16_t mv)
{
    uint8_t code = 0u;
    uint16_t reg;
    npm1300_status_t st = npm1300_check_bus(dev);
    if (st != NPM1300_OK) {
        return st;
    }
    if (idx != NPM1300_LDSW1 && idx != NPM1300_LDSW2) {
        return NPM1300_ERROR_PARAM;
    }

    st = npm1300_mv_to_buck_code(mv, &code); /* same 1.0–3.3 V / 0.1 V table as BUCK */
    if (st != NPM1300_OK) {
        return st;
    }

    reg = (idx == NPM1300_LDSW1) ? NPM1300_REG_LDSW1VOUTSEL : NPM1300_REG_LDSW2VOUTSEL;
    return npm1300_reg_write(dev, reg, code);
}

npm1300_status_t npm1300_ldsw_enable(npm1300_t *dev, npm1300_ldsw_t idx, bool on)
{
    uint16_t reg;
    npm1300_status_t st = npm1300_check_bus(dev);
    if (st != NPM1300_OK) {
        return st;
    }
    if (idx != NPM1300_LDSW1 && idx != NPM1300_LDSW2) {
        return NPM1300_ERROR_PARAM;
    }

    if (idx == NPM1300_LDSW1) {
        reg = on ? NPM1300_REG_LDSW1SET : NPM1300_REG_LDSW1CLR;
    } else {
        reg = on ? NPM1300_REG_LDSW2SET : NPM1300_REG_LDSW2CLR;
    }
    return npm1300_reg_write(dev, reg, NPM1300_TASK_PULSE);
}

npm1300_status_t npm1300_ldsw_get_status(npm1300_t *dev, npm1300_ldsw_t idx, uint8_t *status)
{
    npm1300_status_t st = npm1300_check_bus(dev);
    if (st != NPM1300_OK) {
        return st;
    }
    if (status == NULL || (idx != NPM1300_LDSW1 && idx != NPM1300_LDSW2)) {
        return NPM1300_ERROR_PARAM;
    }
    (void)idx; /* shared LDSWSTATUS */
    return npm1300_reg_read(dev, NPM1300_REG_LDSWSTATUS, status);
}

static const uint16_t npm1300_vterm_mv_table[] = {
    3500u, 3550u, 3600u, 3650u, 4000u, 4050u, 4100u, 4150u,
    4200u, 4250u, 4300u, 4350u, 4400u, 4450u
};

npm1300_status_t npm1300_charger_set_ichg_ma(npm1300_t *dev, uint16_t ma)
{
    uint32_t code;
    uint8_t msb;
    uint8_t lsb;
    npm1300_status_t st = npm1300_check_bus(dev);
    if (st != NPM1300_OK) {
        return st;
    }
    if (ma < 32u || ma > 800u || (ma % 2u) != 0u) {
        return NPM1300_ERROR_RANGE;
    }

    code = ((uint32_t)ma * 1000u) / 2000u;
    msb = (uint8_t)(code >> 1);
    lsb = (uint8_t)(code & 1u);

    st = npm1300_reg_write(dev, NPM1300_REG_BCHGISETMSB, msb);
    if (st != NPM1300_OK) {
        return st;
    }
    return npm1300_reg_write(dev, NPM1300_REG_BCHGISETLSB, lsb);
}

npm1300_status_t npm1300_charger_set_vterm_mv(npm1300_t *dev, uint16_t mv)
{
    uint8_t i;
    npm1300_status_t st = npm1300_check_bus(dev);
    if (st != NPM1300_OK) {
        return st;
    }

    for (i = 0u; i < (uint8_t)(sizeof(npm1300_vterm_mv_table) / sizeof(npm1300_vterm_mv_table[0])); i++) {
        if (npm1300_vterm_mv_table[i] == mv) {
            return npm1300_reg_write(dev, NPM1300_REG_BCHGVTERM, i);
        }
    }
    return NPM1300_ERROR_RANGE;
}

npm1300_status_t npm1300_charger_enable(npm1300_t *dev, bool on)
{
    uint16_t reg;
    npm1300_status_t st = npm1300_check_bus(dev);
    if (st != NPM1300_OK) {
        return st;
    }
    reg = on ? NPM1300_REG_BCHGENABLESET : NPM1300_REG_BCHGENABLECLR;
    return npm1300_reg_write(dev, reg, NPM1300_TASK_PULSE);
}

npm1300_status_t npm1300_charger_get_status(npm1300_t *dev, uint8_t *status)
{
    npm1300_status_t st = npm1300_check_bus(dev);
    if (st != NPM1300_OK) {
        return st;
    }
    if (status == NULL) {
        return NPM1300_ERROR_PARAM;
    }
    return npm1300_reg_read(dev, NPM1300_REG_BCHGCHARGESTATUS, status);
}

npm1300_status_t npm1300_ship_enter(npm1300_t *dev)
{
    npm1300_status_t st = npm1300_check_bus(dev);
    if (st != NPM1300_OK) {
        return st;
    }
    /* After success the device may power down; further I2C is undefined. */
    return npm1300_reg_write(dev, NPM1300_REG_TASKENTERSHIPMODE, NPM1300_TASK_PULSE);
}

npm1300_status_t npm1300_event_read(npm1300_t *dev, uint8_t *mask)
{
    npm1300_status_t st = npm1300_check_bus(dev);
    if (st != NPM1300_OK) {
        return st;
    }
    if (mask == NULL) {
        return NPM1300_ERROR_PARAM;
    }
    return npm1300_reg_read(dev, NPM1300_REG_EVENTSVBUSIN0SET, mask);
}

npm1300_status_t npm1300_event_clear(npm1300_t *dev, uint8_t mask)
{
    npm1300_status_t st = npm1300_check_bus(dev);
    if (st != NPM1300_OK) {
        return st;
    }
    return npm1300_reg_write(dev, NPM1300_REG_EVENTSVBUSIN0CLR, mask);
}
