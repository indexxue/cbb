/**
 * @file    zb25vq16.c
 * @brief   ZB25VQ16 SPI NOR driver (W25Q-class command set)
 */

#include "zb25vq16.h"
#include "zb25vq16_port.h"

#include <string.h>

#define ZB25_STATUS_WIP                 0x01U
#define ZB25_SR_BP_MASK                 0x3CU
#define ZB25_CMD_WRITE_STATUS1          0x01U
#define ZB25_READY_TIMEOUT_MS           5000U
#define ZB25_PAGE_PROG_TIMEOUT_MS       100U
#define ZB25_SECTOR_ERASE_TIMEOUT_MS    3000U
#define ZB25_POLL_CHUNK                 64U

static bool s_initialized;

static bool jedec_id_valid(const uint8_t id[3])
{
    if (id[0] == 0x00U || id[0] == 0xFFU) {
        return false;
    }
    if (id[2] != ZB25VQ16_JEDEC_CAPACITY) {
        return false;
    }
    if (id[0] != ZB25VQ16_JEDEC_MANUFACTURER_WINBOND &&
        id[0] != ZB25VQ16_JEDEC_MANUFACTURER_ZBIT) {
        return false;
    }
    return true;
}

static zb25vq16_status_t cs_xfer(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    zb25vq16_port_cs_low();
    if (zb25vq16_port_xfer(tx, rx, len) != 0) {
        zb25vq16_port_cs_high();
        return ZB25_ERR_SPI;
    }
    zb25vq16_port_cs_high();
    return ZB25_OK;
}

static zb25vq16_status_t wait_ready(uint32_t timeout_ms)
{
    const uint8_t tx[2] = { ZB25_CMD_READ_STATUS1, 0x00U };
    uint8_t rx[2];
    uint8_t status = 0x01U;
    uint32_t start = zb25vq16_port_get_tick();

    while ((status & ZB25_STATUS_WIP) != 0U) {
        zb25vq16_status_t st;

        zb25vq16_port_cs_low();
        if (zb25vq16_port_xfer(tx, rx, 2U) != 0) {
            zb25vq16_port_cs_high();
            return ZB25_ERR_SPI;
        }
        zb25vq16_port_cs_high();
        status = rx[1];
        if ((zb25vq16_port_get_tick() - start) >= timeout_ms) {
            return ZB25_ERR_TIMEOUT;
        }
    }
    return ZB25_OK;
}

static zb25vq16_status_t write_enable(void)
{
    const uint8_t cmd = ZB25_CMD_WRITE_ENABLE;

    return cs_xfer(&cmd, NULL, 1U);
}

static zb25vq16_status_t read_status1(uint8_t *sr)
{
    const uint8_t tx[2] = { ZB25_CMD_READ_STATUS1, 0x00U };
    uint8_t rx[2];

    if (sr == NULL) {
        return ZB25_ERR_PARAM;
    }
    zb25vq16_port_cs_low();
    if (zb25vq16_port_xfer(tx, rx, 2U) != 0) {
        zb25vq16_port_cs_high();
        return ZB25_ERR_SPI;
    }
    zb25vq16_port_cs_high();
    *sr = rx[1];
    return ZB25_OK;
}

static zb25vq16_status_t write_enable_verify(void)
{
    uint8_t sr;
    zb25vq16_status_t st;

    st = write_enable();
    if (st != ZB25_OK) {
        return st;
    }
    st = read_status1(&sr);
    if (st != ZB25_OK) {
        return st;
    }
    if ((sr & 0x02U) == 0U) {
        return ZB25_ERR_WEL;
    }
    return ZB25_OK;
}

static zb25vq16_status_t clear_block_protect(void)
{
    uint8_t sr;
    uint8_t tx[2];
    uint8_t rx[2];
    zb25vq16_status_t st;

    st = read_status1(&sr);
    if (st != ZB25_OK) {
        return st;
    }
    if ((sr & ZB25_SR_BP_MASK) == 0U) {
        return ZB25_OK;
    }

    st = write_enable();
    if (st != ZB25_OK) {
        return st;
    }

    tx[0] = ZB25_CMD_WRITE_STATUS1;
    tx[1] = 0x00U;
    zb25vq16_port_cs_low();
    if (zb25vq16_port_xfer(tx, rx, 2U) != 0) {
        zb25vq16_port_cs_high();
        return ZB25_ERR_SPI;
    }
    zb25vq16_port_cs_high();

    return wait_ready(ZB25_PAGE_PROG_TIMEOUT_MS);
}

zb25vq16_status_t zb25vq16_init(void)
{
    uint8_t id[3];

    s_initialized = false;
    zb25vq16_port_cs_high();
    zb25vq16_port_delay_ms(1U);

    {
        const uint8_t pd = ZB25_CMD_RELEASE_POWERDOWN;
        (void)cs_xfer(&pd, NULL, 1U);
        zb25vq16_port_delay_ms(1U);
    }

    if (zb25vq16_read_jedec_id(id) != ZB25_OK) {
        return ZB25_ERR_ID;
    }
    if (!jedec_id_valid(id)) {
        return ZB25_ERR_ID;
    }

    if (clear_block_protect() != ZB25_OK) {
        return ZB25_ERR_SPI;
    }

    s_initialized = true;
    return ZB25_OK;
}

bool zb25vq16_is_initialized(void)
{
    return s_initialized;
}

zb25vq16_status_t zb25vq16_read_jedec_id(uint8_t id[3])
{
    const uint8_t tx[4] = { ZB25_CMD_JEDEC_ID, 0x00U, 0x00U, 0x00U };
    uint8_t rx[4];

    if (id == NULL) {
        return ZB25_ERR_PARAM;
    }

    zb25vq16_port_cs_low();
    if (zb25vq16_port_xfer(tx, rx, (uint16_t)sizeof(tx)) != 0) {
        zb25vq16_port_cs_high();
        return ZB25_ERR_SPI;
    }
    zb25vq16_port_cs_high();

    id[0] = rx[1];
    id[1] = rx[2];
    id[2] = rx[3];
    return ZB25_OK;
}

zb25vq16_status_t zb25vq16_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint8_t hdr[4];

    if (!s_initialized || buf == NULL || len == 0U) {
        return ZB25_ERR_PARAM;
    }
    if ((addr + len) > ZB25VQ16_FLASH_SIZE) {
        return ZB25_ERR_PARAM;
    }

    hdr[0] = ZB25_CMD_READ_DATA;
    hdr[1] = (uint8_t)(addr >> 16);
    hdr[2] = (uint8_t)(addr >> 8);
    hdr[3] = (uint8_t)addr;

    {
        uint8_t discard[4];

        zb25vq16_port_cs_low();
        if (zb25vq16_port_xfer(hdr, discard, 4U) != 0) {
            zb25vq16_port_cs_high();
            return ZB25_ERR_SPI;
        }
    }
    if (zb25vq16_port_read_stream(buf, len) != 0) {
        zb25vq16_port_cs_high();
        return ZB25_ERR_SPI;
    }
    zb25vq16_port_cs_high();
    return ZB25_OK;
}

static zb25vq16_status_t program_page(uint32_t addr, const uint8_t *buf, uint16_t len)
{
    uint8_t cmd[4 + ZB25VQ16_PAGE_SIZE];
    zb25vq16_status_t st;

    if (len == 0U || len > ZB25VQ16_PAGE_SIZE) {
        return ZB25_ERR_PARAM;
    }

    st = write_enable_verify();
    if (st != ZB25_OK) {
        return st;
    }

    cmd[0] = ZB25_CMD_PAGE_PROGRAM;
    cmd[1] = (uint8_t)(addr >> 16);
    cmd[2] = (uint8_t)(addr >> 8);
    cmd[3] = (uint8_t)addr;
    memcpy(&cmd[4], buf, len);

    {
        uint8_t rx[4U + ZB25VQ16_PAGE_SIZE];

        zb25vq16_port_cs_low();
        if (zb25vq16_port_xfer(cmd, rx, (uint16_t)(4U + len)) != 0) {
            zb25vq16_port_cs_high();
            return ZB25_ERR_SPI;
        }
        zb25vq16_port_cs_high();
    }

    return wait_ready(ZB25_PAGE_PROG_TIMEOUT_MS);
}

zb25vq16_status_t zb25vq16_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    if (!s_initialized || buf == NULL || len == 0U) {
        return ZB25_ERR_PARAM;
    }
    if ((addr + len) > ZB25VQ16_FLASH_SIZE) {
        return ZB25_ERR_PARAM;
    }

    while (len > 0U) {
        uint32_t page_off = addr % ZB25VQ16_PAGE_SIZE;
        uint32_t chunk = ZB25VQ16_PAGE_SIZE - page_off;

        if (chunk > len) {
            chunk = len;
        }
        if (program_page(addr, buf, (uint16_t)chunk) != ZB25_OK) {
            return ZB25_ERR_SPI;
        }
        addr += chunk;
        buf += chunk;
        len -= chunk;
    }
    return ZB25_OK;
}

zb25vq16_status_t zb25vq16_erase_sector(uint32_t sector_index)
{
    uint32_t addr;
    uint8_t cmd[4];
    zb25vq16_status_t st;

    if (!s_initialized) {
        return ZB25_ERR_NOT_INIT;
    }
    if (sector_index >= ZB25VQ16_SECTOR_COUNT) {
        return ZB25_ERR_PARAM;
    }

    addr = sector_index * ZB25VQ16_SECTOR_SIZE;
    st = write_enable_verify();
    if (st != ZB25_OK) {
        return st;
    }

    cmd[0] = ZB25_CMD_SECTOR_ERASE;
    cmd[1] = (uint8_t)(addr >> 16);
    cmd[2] = (uint8_t)(addr >> 8);
    cmd[3] = (uint8_t)addr;

    {
        uint8_t rx[4];

        zb25vq16_port_cs_low();
        if (zb25vq16_port_xfer(cmd, rx, (uint16_t)sizeof(cmd)) != 0) {
            zb25vq16_port_cs_high();
            return ZB25_ERR_SPI;
        }
        zb25vq16_port_cs_high();
    }

    return wait_ready(ZB25_SECTOR_ERASE_TIMEOUT_MS);
}
