/**
 * @file    w25q.c
 * @brief   W25Q-family SPI NOR driver (standard SPI command set).
 */

#include "w25q.h"

#include <string.h>

#define W25Q_STATUS_WIP                 0x01U
#define W25Q_STATUS_WEL                 0x02U
#define W25Q_SR_BP_MASK                 0x3CU
#define W25Q_READY_TIMEOUT_MS           5000U
#define W25Q_PAGE_PROG_TIMEOUT_MS       100U
#define W25Q_SECTOR_ERASE_TIMEOUT_MS    3000U
#define W25Q_READ_CHUNK                 64U

static bool cfg_callbacks_ok(const w25q_config_t *cfg)
{
    if (cfg->spi_xfer == NULL || cfg->set_cs == NULL) {
        return false;
    }
    if (cfg->delay_ms == NULL || cfg->get_tick_ms == NULL) {
        return false;
    }
    return true;
}

static bool bus_ready(const w25q_t *dev)
{
    return (dev != NULL) && (dev->spi_xfer != NULL) && (dev->set_cs != NULL);
}

static bool jedec_capacity_ok(uint8_t capacity)
{
    return (capacity >= W25Q_JEDEC_CAPACITY_MIN) &&
           (capacity <= W25Q_JEDEC_CAPACITY_MAX);
}

static bool jedec_manufacturer_ok(uint8_t manuf)
{
    if (manuf == 0x00U || manuf == 0xFFU) {
        return false;
    }
    if (manuf == W25Q_JEDEC_MANUF_WINBOND || manuf == W25Q_JEDEC_MANUF_ZBIT) {
        return true;
    }
    return false;
}

static bool apply_jedec_geometry(w25q_t *dev, const uint8_t id[3], bool skip_id_check)
{
    if (!jedec_capacity_ok(id[2])) {
        return false;
    }
    if (!skip_id_check && !jedec_manufacturer_ok(id[0])) {
        return false;
    }
    if (id[0] == 0x00U || id[0] == 0xFFU) {
        return false;
    }

    dev->jedec[0] = id[0];
    dev->jedec[1] = id[1];
    dev->jedec[2] = id[2];
    dev->flash_size = 1U << id[2];
    dev->sector_count = dev->flash_size / W25Q_SECTOR_SIZE;
    return true;
}

static w25q_status_t cs_xfer(w25q_t *dev, const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    dev->set_cs(0U);
    if (dev->spi_xfer(tx, rx, len) != 0) {
        dev->set_cs(1U);
        return W25Q_ERR_BUS;
    }
    dev->set_cs(1U);
    return W25Q_OK;
}

static w25q_status_t wait_ready(w25q_t *dev, uint32_t timeout_ms)
{
    const uint8_t tx[2] = { W25Q_CMD_READ_STATUS1, 0x00U };
    uint8_t rx[2];
    uint8_t status = W25Q_STATUS_WIP;
    uint32_t start = dev->get_tick_ms();
    uint32_t polls = 0U;

    while ((status & W25Q_STATUS_WIP) != 0U) {
        dev->set_cs(0U);
        if (dev->spi_xfer(tx, rx, 2U) != 0) {
            dev->set_cs(1U);
            return W25Q_ERR_BUS;
        }
        dev->set_cs(1U);
        status = rx[1];
        /* Floating MISO often reads 0xFF — do not spin for full timeout. */
        if (status == 0xFFU) {
            return W25Q_ERR_BUS;
        }
        polls++;
        if ((polls > 10000U) ||
            ((dev->get_tick_ms() - start) >= timeout_ms)) {
            return W25Q_ERR_TIMEOUT;
        }
    }
    return W25Q_OK;
}

static w25q_status_t read_status1(w25q_t *dev, uint8_t *sr)
{
    const uint8_t tx[2] = { W25Q_CMD_READ_STATUS1, 0x00U };
    uint8_t rx[2];

    if (sr == NULL) {
        return W25Q_ERR_PARAM;
    }

    dev->set_cs(0U);
    if (dev->spi_xfer(tx, rx, 2U) != 0) {
        dev->set_cs(1U);
        return W25Q_ERR_BUS;
    }
    dev->set_cs(1U);
    *sr = rx[1];
    return W25Q_OK;
}

static w25q_status_t write_enable(w25q_t *dev)
{
    const uint8_t cmd = W25Q_CMD_WRITE_ENABLE;

    return cs_xfer(dev, &cmd, NULL, 1U);
}

static w25q_status_t write_enable_verify(w25q_t *dev)
{
    uint8_t sr;
    w25q_status_t st;

    st = write_enable(dev);
    if (st != W25Q_OK) {
        return st;
    }
    st = read_status1(dev, &sr);
    if (st != W25Q_OK) {
        return st;
    }
    if ((sr & W25Q_STATUS_WEL) == 0U) {
        return W25Q_ERR_WEL;
    }
    return W25Q_OK;
}

static w25q_status_t clear_block_protect(w25q_t *dev)
{
    uint8_t sr;
    uint8_t tx[2];
    uint8_t rx[2];
    w25q_status_t st;

    st = read_status1(dev, &sr);
    if (st != W25Q_OK) {
        return st;
    }
    if (sr == 0xFFU) {
        return W25Q_ERR_BUS;
    }
    if ((sr & W25Q_SR_BP_MASK) == 0U) {
        return W25Q_OK;
    }

    st = write_enable(dev);
    if (st != W25Q_OK) {
        return st;
    }

    tx[0] = W25Q_CMD_WRITE_STATUS1;
    tx[1] = 0x00U;
    st = cs_xfer(dev, tx, rx, 2U);
    if (st != W25Q_OK) {
        return st;
    }
    return wait_ready(dev, W25Q_PAGE_PROG_TIMEOUT_MS);
}

static w25q_status_t read_payload(w25q_t *dev, uint8_t *buf, uint32_t len)
{
    uint8_t zeros[W25Q_READ_CHUNK];

    if (dev->read_stream != NULL) {
        if (dev->read_stream(buf, len) != 0) {
            return W25Q_ERR_BUS;
        }
        return W25Q_OK;
    }

    memset(zeros, 0, sizeof(zeros));
    while (len > 0U) {
        uint16_t chunk = (len > W25Q_READ_CHUNK) ? (uint16_t)W25Q_READ_CHUNK
                                                  : (uint16_t)len;

        if (dev->spi_xfer(zeros, buf, chunk) != 0) {
            return W25Q_ERR_BUS;
        }
        buf += chunk;
        len -= chunk;
    }
    return W25Q_OK;
}

static w25q_status_t program_page(w25q_t *dev, uint32_t addr,
                                  const uint8_t *buf, uint16_t len)
{
    uint8_t cmd[4U + W25Q_PAGE_SIZE];
    uint8_t rx[4U + W25Q_PAGE_SIZE];
    w25q_status_t st;

    if (len == 0U || len > W25Q_PAGE_SIZE) {
        return W25Q_ERR_PARAM;
    }

    st = write_enable_verify(dev);
    if (st != W25Q_OK) {
        return st;
    }

    cmd[0] = W25Q_CMD_PAGE_PROGRAM;
    cmd[1] = (uint8_t)(addr >> 16);
    cmd[2] = (uint8_t)(addr >> 8);
    cmd[3] = (uint8_t)addr;
    memcpy(&cmd[4], buf, len);

    st = cs_xfer(dev, cmd, rx, (uint16_t)(4U + len));
    if (st != W25Q_OK) {
        return st;
    }
    return wait_ready(dev, W25Q_PAGE_PROG_TIMEOUT_MS);
}

static void copy_config(w25q_t *dev, const w25q_config_t *cfg)
{
    memset(dev, 0, sizeof(*dev));
    dev->spi_xfer = cfg->spi_xfer;
    dev->set_cs = cfg->set_cs;
    dev->delay_ms = cfg->delay_ms;
    dev->get_tick_ms = cfg->get_tick_ms;
    dev->read_stream = cfg->read_stream;
    dev->skip_id_check = cfg->skip_id_check;
}

w25q_status_t w25q_init(w25q_t *dev, const w25q_config_t *cfg)
{
    uint8_t id[3];
    const uint8_t release_pd = W25Q_CMD_RELEASE_POWERDOWN;

    if (dev == NULL || cfg == NULL || !cfg_callbacks_ok(cfg)) {
        return W25Q_ERR_PARAM;
    }

    copy_config(dev, cfg);
    dev->set_cs(1U);
    dev->delay_ms(1U);

    (void)cs_xfer(dev, &release_pd, NULL, 1U);
    dev->delay_ms(1U);

    if (w25q_read_jedec_id(dev, id) != W25Q_OK) {
        return W25Q_ERR_ID;
    }
    if (!apply_jedec_geometry(dev, id, cfg->skip_id_check)) {
        return W25Q_ERR_ID;
    }
    if (clear_block_protect(dev) != W25Q_OK) {
        return W25Q_ERR_BUS;
    }

    dev->initialized = true;
    return W25Q_OK;
}

w25q_status_t w25q_read_jedec_id(w25q_t *dev, uint8_t id[3])
{
    const uint8_t tx[4] = { W25Q_CMD_JEDEC_ID, 0x00U, 0x00U, 0x00U };
    uint8_t rx[4];

    if (!bus_ready(dev) || id == NULL) {
        return W25Q_ERR_PARAM;
    }

    if (cs_xfer(dev, tx, rx, (uint16_t)sizeof(tx)) != W25Q_OK) {
        return W25Q_ERR_BUS;
    }

    id[0] = rx[1];
    id[1] = rx[2];
    id[2] = rx[3];
    return W25Q_OK;
}

uint32_t w25q_get_size(const w25q_t *dev)
{
    if (dev == NULL || !dev->initialized) {
        return 0U;
    }
    return dev->flash_size;
}

uint32_t w25q_get_sector_count(const w25q_t *dev)
{
    if (dev == NULL || !dev->initialized) {
        return 0U;
    }
    return dev->sector_count;
}

bool w25q_is_initialized(const w25q_t *dev)
{
    return (dev != NULL) && dev->initialized;
}

w25q_status_t w25q_read(w25q_t *dev, uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint8_t hdr[4];
    uint8_t discard[4];
    w25q_status_t st;

    if (dev == NULL || !dev->initialized) {
        return W25Q_ERR_NOT_INIT;
    }
    if (buf == NULL || len == 0U) {
        return W25Q_ERR_PARAM;
    }
    if ((addr + len) > dev->flash_size) {
        return W25Q_ERR_PARAM;
    }

    hdr[0] = W25Q_CMD_READ_DATA;
    hdr[1] = (uint8_t)(addr >> 16);
    hdr[2] = (uint8_t)(addr >> 8);
    hdr[3] = (uint8_t)addr;

    dev->set_cs(0U);
    if (dev->spi_xfer(hdr, discard, 4U) != 0) {
        dev->set_cs(1U);
        return W25Q_ERR_BUS;
    }
    st = read_payload(dev, buf, len);
    dev->set_cs(1U);
    return st;
}

w25q_status_t w25q_write(w25q_t *dev, uint32_t addr, const uint8_t *buf, uint32_t len)
{
    if (dev == NULL || !dev->initialized) {
        return W25Q_ERR_NOT_INIT;
    }
    if (buf == NULL || len == 0U) {
        return W25Q_ERR_PARAM;
    }
    if ((addr + len) > dev->flash_size) {
        return W25Q_ERR_PARAM;
    }

    while (len > 0U) {
        uint32_t page_off = addr % W25Q_PAGE_SIZE;
        uint32_t chunk = W25Q_PAGE_SIZE - page_off;
        w25q_status_t st;

        if (chunk > len) {
            chunk = len;
        }
        st = program_page(dev, addr, buf, (uint16_t)chunk);
        if (st != W25Q_OK) {
            return st;
        }
        addr += chunk;
        buf += chunk;
        len -= chunk;
    }
    return W25Q_OK;
}

w25q_status_t w25q_erase_sector(w25q_t *dev, uint32_t sector_index)
{
    uint32_t addr;
    uint8_t cmd[4];
    uint8_t rx[4];
    w25q_status_t st;

    if (dev == NULL || !dev->initialized) {
        return W25Q_ERR_NOT_INIT;
    }
    if (sector_index >= dev->sector_count) {
        return W25Q_ERR_PARAM;
    }

    addr = sector_index * W25Q_SECTOR_SIZE;
    st = write_enable_verify(dev);
    if (st != W25Q_OK) {
        return st;
    }

    cmd[0] = W25Q_CMD_SECTOR_ERASE;
    cmd[1] = (uint8_t)(addr >> 16);
    cmd[2] = (uint8_t)(addr >> 8);
    cmd[3] = (uint8_t)addr;

    st = cs_xfer(dev, cmd, rx, (uint16_t)sizeof(cmd));
    if (st != W25Q_OK) {
        return st;
    }
    return wait_ready(dev, W25Q_SECTOR_ERASE_TIMEOUT_MS);
}
