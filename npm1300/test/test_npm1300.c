/**
 * @file test_npm1300.c
 * @brief Host mock I2C tests for cbb/npm1300 (gcc).
 */

#include "npm1300.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t g_regs[0x10000];
static unsigned g_write_count;
static int g_fail_bus;

static void mock_reset(void)
{
    memset(g_regs, 0, sizeof(g_regs));
    g_write_count = 0u;
    g_fail_bus = 0;
}

static int mock_write(uint8_t addr7, const uint8_t *data, uint16_t len)
{
    if (g_fail_bus) {
        return -1;
    }
    if (addr7 != 0x6Bu || data == NULL || len < 3u) {
        return -1;
    }
    g_write_count++;
    {
        uint16_t addr = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
        uint16_t i;
        for (i = 2u; i < len; i++) {
            g_regs[addr + (uint16_t)(i - 2u)] = data[i];
        }
    }
    return 0;
}

static int mock_write_read(uint8_t addr7,
                           const uint8_t *tx,
                           uint16_t tx_len,
                           uint8_t *rx,
                           uint16_t rx_len)
{
    if (g_fail_bus) {
        return -1;
    }
    if (addr7 != 0x6Bu || tx == NULL || rx == NULL || tx_len != 2u || rx_len == 0u) {
        return -1;
    }
    {
        uint16_t addr = (uint16_t)(((uint16_t)tx[0] << 8) | tx[1]);
        uint16_t i;
        for (i = 0u; i < rx_len; i++) {
            rx[i] = g_regs[addr + i];
        }
    }
    return 0;
}

static void expect_eq_int(const char *name, int got, int want)
{
    if (got != want) {
        fprintf(stderr, "FAIL %s: got %d want %d\n", name, got, want);
        exit(1);
    }
}

static void expect_true(const char *name, int cond)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", name);
        exit(1);
    }
}

static npm1300_config_t make_cfg(void)
{
    npm1300_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.write = mock_write;
    cfg.write_read = mock_write_read;
    cfg.i2c_addr = 0x6Bu;
    return cfg;
}

static void test_task1_bus_and_init(void)
{
    npm1300_t dev;
    npm1300_config_t cfg = make_cfg();
    uint8_t val = 0u;

    mock_reset();
    expect_eq_int("init null", (int)npm1300_init(NULL, &cfg), (int)NPM1300_ERROR_PARAM);

    mock_reset();
    memset(&dev, 0, sizeof(dev));
    g_regs[0x0434] = 0xA5u;
    expect_eq_int("init ok", (int)npm1300_init(&dev, &cfg), (int)NPM1300_OK);
    expect_true("initialized", dev.initialized);
    expect_eq_int("init zero writes", (int)g_write_count, 0);

    expect_eq_int("reg_write", (int)npm1300_reg_write(&dev, 0x0408u, 0x14u), (int)NPM1300_OK);
    expect_eq_int("image 0x0408", (int)g_regs[0x0408], 0x14);
    expect_eq_int("reg_read", (int)npm1300_reg_read(&dev, 0x0408u, &val), (int)NPM1300_OK);
    expect_eq_int("readback", (int)val, 0x14);

    mock_reset();
    memset(&dev, 0, sizeof(dev));
    g_fail_bus = 1;
    expect_eq_int("init i2c fail", (int)npm1300_init(&dev, &cfg), (int)NPM1300_ERROR_I2C);
    expect_true("not init after fail", !dev.initialized);
}

static void test_task2_buck(void)
{
    npm1300_t dev;
    npm1300_config_t cfg = make_cfg();
    uint8_t st = 0u;

    mock_reset();
    memset(&dev, 0, sizeof(dev));
    expect_eq_int("buck init", (int)npm1300_init(&dev, &cfg), (int)NPM1300_OK);

    expect_eq_int("buck 3v0", (int)npm1300_buck_set_vout_mv(&dev, NPM1300_BUCK1, 3000u), (int)NPM1300_OK);
    expect_eq_int("normvout", (int)g_regs[0x0408], 0x14);
    expect_eq_int("swctrl buck1", (int)(g_regs[0x040F] & 0x01u), 0x01);

    expect_eq_int("buck range", (int)npm1300_buck_set_vout_mv(&dev, NPM1300_BUCK1, 3010u),
                  (int)NPM1300_ERROR_RANGE);

    expect_eq_int("buck en", (int)npm1300_buck_enable(&dev, NPM1300_BUCK1, true), (int)NPM1300_OK);
    expect_eq_int("ena set", (int)g_regs[0x0400], 0x01);
    expect_eq_int("buck dis", (int)npm1300_buck_enable(&dev, NPM1300_BUCK1, false), (int)NPM1300_OK);
    expect_eq_int("ena clr", (int)g_regs[0x0401], 0x01);

    expect_eq_int("bad buck", (int)npm1300_buck_enable(&dev, (npm1300_buck_t)0, true),
                  (int)NPM1300_ERROR_PARAM);

    g_regs[0x0434] = 0x3Cu;
    expect_eq_int("buck status", (int)npm1300_buck_get_status(&dev, NPM1300_BUCK1, &st), (int)NPM1300_OK);
    expect_eq_int("status val", (int)st, 0x3C);
}

static void test_task3_ldsw(void)
{
    npm1300_t dev;
    npm1300_config_t cfg = make_cfg();
    uint8_t st = 0u;

    mock_reset();
    memset(&dev, 0, sizeof(dev));
    expect_eq_int("ldsw init", (int)npm1300_init(&dev, &cfg), (int)NPM1300_OK);

    expect_eq_int("ldo mode", (int)npm1300_ldsw_set_mode(&dev, NPM1300_LDSW1, NPM1300_LDSW_MODE_LDO),
                  (int)NPM1300_OK);
    expect_eq_int("ldosel1", (int)g_regs[0x0808], 0x01);
    expect_eq_int("loadsw mode", (int)npm1300_ldsw_set_mode(&dev, NPM1300_LDSW1, NPM1300_LDSW_MODE_LOADSW),
                  (int)NPM1300_OK);
    expect_eq_int("ldosel1 ls", (int)g_regs[0x0808], 0x00);

    expect_eq_int("ldo 3v0", (int)npm1300_ldsw_set_vout_mv(&dev, NPM1300_LDSW1, 3000u), (int)NPM1300_OK);
    expect_eq_int("voutsel1", (int)g_regs[0x080C], 0x14);
    expect_eq_int("ldo range", (int)npm1300_ldsw_set_vout_mv(&dev, NPM1300_LDSW1, 3010u),
                  (int)NPM1300_ERROR_RANGE);

    expect_eq_int("ls2 en", (int)npm1300_ldsw_enable(&dev, NPM1300_LDSW2, true), (int)NPM1300_OK);
    expect_eq_int("ls2 set", (int)g_regs[0x0802], 0x01);
    expect_eq_int("ls2 dis", (int)npm1300_ldsw_enable(&dev, NPM1300_LDSW2, false), (int)NPM1300_OK);
    expect_eq_int("ls2 clr", (int)g_regs[0x0803], 0x01);

    g_regs[0x0804] = 0x55u;
    expect_eq_int("ldsw status", (int)npm1300_ldsw_get_status(&dev, NPM1300_LDSW2, &st), (int)NPM1300_OK);
    expect_eq_int("ldsw st val", (int)st, 0x55);
}

static void test_task4_charger(void)
{
    npm1300_t dev;
    npm1300_config_t cfg = make_cfg();
    uint8_t st = 0u;

    mock_reset();
    memset(&dev, 0, sizeof(dev));
    expect_eq_int("chg init", (int)npm1300_init(&dev, &cfg), (int)NPM1300_OK);

    expect_eq_int("ichg 200", (int)npm1300_charger_set_ichg_ma(&dev, 200u), (int)NPM1300_OK);
    expect_eq_int("iset msb 200", (int)g_regs[0x0308], 0x32);
    expect_eq_int("iset lsb 200", (int)g_regs[0x0309], 0x00);

    expect_eq_int("ichg 300", (int)npm1300_charger_set_ichg_ma(&dev, 300u), (int)NPM1300_OK);
    expect_eq_int("iset msb 300", (int)g_regs[0x0308], 0x4B);
    expect_eq_int("iset lsb 300", (int)g_regs[0x0309], 0x00);

    expect_eq_int("ichg range", (int)npm1300_charger_set_ichg_ma(&dev, 201u), (int)NPM1300_ERROR_RANGE);
    expect_eq_int("ichg low", (int)npm1300_charger_set_ichg_ma(&dev, 30u), (int)NPM1300_ERROR_RANGE);

    expect_eq_int("vterm 4200", (int)npm1300_charger_set_vterm_mv(&dev, 4200u), (int)NPM1300_OK);
    expect_eq_int("vterm reg", (int)g_regs[0x030C], 0x08);
    expect_eq_int("vterm range", (int)npm1300_charger_set_vterm_mv(&dev, 4210u), (int)NPM1300_ERROR_RANGE);

    expect_eq_int("chg en", (int)npm1300_charger_enable(&dev, true), (int)NPM1300_OK);
    expect_eq_int("en set", (int)g_regs[0x0304], 0x01);
    expect_eq_int("chg dis", (int)npm1300_charger_enable(&dev, false), (int)NPM1300_OK);
    expect_eq_int("en clr", (int)g_regs[0x0305], 0x01);

    g_regs[0x0334] = 0x11u;
    expect_eq_int("chg status", (int)npm1300_charger_get_status(&dev, &st), (int)NPM1300_OK);
    expect_eq_int("chg st val", (int)st, 0x11);
}

int main(void)
{
    test_task1_bus_and_init();
    test_task2_buck();
    test_task3_ldsw();
    test_task4_charger();
    puts("ok");
    return 0;
}
