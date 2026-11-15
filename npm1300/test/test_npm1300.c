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

int main(void)
{
    test_task1_bus_and_init();
    puts("ok");
    return 0;
}
