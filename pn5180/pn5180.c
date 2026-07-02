/**
 * @file    pn5180.c
 * @brief   PN5180 SPI 主机接口驱动实现
 */

#include "pn5180.h"

#include <stddef.h>
#include <string.h>

static pn5180_status_t pn5180_read_eeprom_raw(pn5180_t *dev, uint8_t addr, uint8_t *data, uint8_t len);

static pn5180_status_t pn5180_check_callbacks(pn5180_t *dev)
{
    if (dev == NULL)
    {
        return PN5180_ERROR_PARAM;
    }
    if (dev->spi_tx_rx == NULL || dev->set_nss == NULL || dev->read_busy == NULL)
    {
        return PN5180_ERROR_PARAM;
    }
    return PN5180_OK;
}

static pn5180_status_t pn5180_check_ready(pn5180_t *dev)
{
    pn5180_status_t st = pn5180_check_callbacks(dev);
    if (st != PN5180_OK)
    {
        return st;
    }
    if (!dev->initialized)
    {
        return PN5180_ERROR_NOT_INIT;
    }
    return PN5180_OK;
}

static int pn5180_busy_level(pn5180_t *dev, bool busy)
{
    if (dev->busy_active_high)
    {
        return busy ? 1 : 0;
    }
    return busy ? 0 : 1;
}

#define PN5180_NSS_SETUP_MS       2u
/** NXP 11.4.1 step 3: wait BUSY high before NSS release is optional in normal mode. */
#define PN5180_BUSY_HIGH_WAIT_MS  50u

static void pn5180_nss_setup_delay(pn5180_t *dev)
{
    if (dev->delay_ms != NULL)
    {
        dev->delay_ms(PN5180_NSS_SETUP_MS);
    }
}

static pn5180_status_t pn5180_wait_busy_level(pn5180_t *dev, bool busy, uint32_t timeout_ms)
{
    if (dev->read_busy == NULL)
    {
        return PN5180_ERROR_PARAM;
    }

    const int target = pn5180_busy_level(dev, busy);
    uint32_t elapsed = 0u;
    uint32_t limit = timeout_ms;

    if (limit == 0u)
    {
        limit = 300u;
    }

    while (dev->read_busy() != target)
    {
        if (dev->delay_ms != NULL)
        {
            dev->delay_ms(1u);
        }
        elapsed++;
        if (elapsed >= limit)
        {
            return PN5180_ERROR_BUSY;
        }
    }
    return PN5180_OK;
}

/** Step 3 (BUSY high before NSS deassert): best-effort, do not fail transceive. */
static void pn5180_wait_busy_high_optional(pn5180_t *dev)
{
    (void)pn5180_wait_busy_level(dev, true, PN5180_BUSY_HIGH_WAIT_MS);
}

static pn5180_status_t pn5180_transceive(pn5180_t *dev,
                                         const uint8_t *tx,
                                         uint16_t tx_len,
                                         uint8_t *rx,
                                         uint16_t rx_len)
{
    pn5180_status_t st = pn5180_check_callbacks(dev);
    if (st != PN5180_OK)
    {
        return st;
    }
    if (tx == NULL || tx_len == 0u)
    {
        return PN5180_ERROR_PARAM;
    }
    if ((rx == NULL) != (rx_len == 0u))
    {
        return PN5180_ERROR_PARAM;
    }

    const uint32_t to = dev->busy_timeout_ms;

    st = pn5180_wait_busy_level(dev, false, to);
    if (st != PN5180_OK)
    {
        return st;
    }

    dev->set_nss(0);
    pn5180_nss_setup_delay(dev);
    if (dev->spi_tx_rx(tx, NULL, tx_len) != 0)
    {
        dev->set_nss(1);
        return PN5180_ERROR_BUS;
    }

    pn5180_wait_busy_high_optional(dev);

    dev->set_nss(1);

    if (dev->delay_ms != NULL)
    {
        dev->delay_ms(1u);
    }

    st = pn5180_wait_busy_level(dev, false, to);
    if (st != PN5180_OK)
    {
        return st;
    }

    if (rx_len == 0u)
    {
        return PN5180_OK;
    }

    dev->set_nss(0);
    pn5180_nss_setup_delay(dev);
    if (dev->spi_tx_rx(NULL, rx, rx_len) != 0)
    {
        dev->set_nss(1);
        return PN5180_ERROR_BUS;
    }

    pn5180_wait_busy_high_optional(dev);

    dev->set_nss(1);

    if (dev->delay_ms != NULL)
    {
        dev->delay_ms(1u);
    }

    return pn5180_wait_busy_level(dev, false, to);
}

static pn5180_status_t pn5180_pack_reg_write(uint8_t cmd,
                                             uint8_t reg,
                                             uint32_t value,
                                             uint8_t out[6])
{
    if (out == NULL)
    {
        return PN5180_ERROR_PARAM;
    }

    out[0] = cmd;
    out[1] = reg;
    out[2] = (uint8_t)(value & 0xFFu);
    out[3] = (uint8_t)((value >> 8) & 0xFFu);
    out[4] = (uint8_t)((value >> 16) & 0xFFu);
    out[5] = (uint8_t)((value >> 24) & 0xFFu);
    return PN5180_OK;
}

pn5180_status_t pn5180_register(pn5180_t *dev, const pn5180_config_t *cfg)
{
    if (dev == NULL || cfg == NULL)
    {
        return PN5180_ERROR_PARAM;
    }
    if (cfg->spi_tx_rx == NULL || cfg->set_nss == NULL || cfg->read_busy == NULL)
    {
        return PN5180_ERROR_PARAM;
    }

    dev->spi_tx_rx        = cfg->spi_tx_rx;
    dev->set_nss          = cfg->set_nss;
    dev->read_busy        = cfg->read_busy;
    dev->set_rst          = cfg->set_rst;
    dev->set_pwr_on       = cfg->set_pwr_on;
    dev->delay_ms         = cfg->delay_ms;
    dev->busy_timeout_ms  = cfg->busy_timeout_ms;
    dev->busy_active_high = cfg->busy_active_high;
    dev->reset_on_init    = cfg->reset_on_init;
    dev->initialized      = false;
    return PN5180_OK;
}

static pn5180_status_t pn5180_validate_id(pn5180_t *dev)
{
    uint8_t fw[2];

    pn5180_status_t st = pn5180_read_firmware_version(dev, fw);
    if (st != PN5180_OK)
    {
        return st;
    }
    if (fw[0] == 0x00u && fw[1] == 0x00u)
    {
        return PN5180_ERROR_ID;
    }
    if (fw[0] == 0xFFu && fw[1] == 0xFFu)
    {
        return PN5180_ERROR_ID;
    }
    return PN5180_OK;
}

pn5180_status_t pn5180_init(pn5180_t *dev)
{
    if (dev == NULL || dev->spi_tx_rx == NULL || dev->set_nss == NULL || dev->read_busy == NULL)
    {
        return PN5180_ERROR_PARAM;
    }

    if (dev->set_pwr_on != NULL)
    {
        dev->set_pwr_on(1);
        if (dev->delay_ms != NULL)
        {
            dev->delay_ms(10u);
        }
    }

    if (dev->set_rst != NULL)
    {
        dev->set_rst(1);
    }
    dev->set_nss(1);

    if (dev->reset_on_init)
    {
        pn5180_status_t st = pn5180_reset(dev);
        if (st != PN5180_OK)
        {
            return st;
        }

        st = pn5180_validate_id(dev);
        if (st != PN5180_OK)
        {
            return st;
        }
    }

    dev->initialized = true;
    return PN5180_OK;
}

pn5180_status_t pn5180_reset(pn5180_t *dev)
{
    if (dev == NULL || dev->set_rst == NULL)
    {
        return PN5180_ERROR_PARAM;
    }
    if (dev->delay_ms == NULL)
    {
        return PN5180_ERROR_PARAM;
    }

    dev->set_rst(0);
    dev->delay_ms(10u);
    dev->set_rst(1);
    dev->delay_ms(50u);

    pn5180_status_t st = pn5180_wait_irq(dev, PN5180_IRQ_IDLE_STAT, 500u);
    if (st != PN5180_OK)
    {
        /* Some boards miss IDLE on first poll; continue and let EEPROM read decide. */
        (void)st;
    }

    return pn5180_clear_irq_status(dev, 0xFFFFFFFFu);
}

pn5180_status_t pn5180_write_register(pn5180_t *dev, uint8_t reg, uint32_t value)
{
    pn5180_status_t st = pn5180_check_callbacks(dev);
    if (st != PN5180_OK)
    {
        return st;
    }

    uint8_t buf[6];
    st = pn5180_pack_reg_write(PN5180_CMD_WRITE_REGISTER, reg, value, buf);
    if (st != PN5180_OK)
    {
        return st;
    }
    return pn5180_transceive(dev, buf, 6u, NULL, 0u);
}

pn5180_status_t pn5180_write_register_or_mask(pn5180_t *dev, uint8_t reg, uint32_t mask)
{
    pn5180_status_t st = pn5180_check_callbacks(dev);
    if (st != PN5180_OK)
    {
        return st;
    }

    uint8_t buf[6];
    st = pn5180_pack_reg_write(PN5180_CMD_WRITE_REGISTER_OR_MASK, reg, mask, buf);
    if (st != PN5180_OK)
    {
        return st;
    }
    return pn5180_transceive(dev, buf, 6u, NULL, 0u);
}

pn5180_status_t pn5180_write_register_and_mask(pn5180_t *dev, uint8_t reg, uint32_t mask)
{
    pn5180_status_t st = pn5180_check_callbacks(dev);
    if (st != PN5180_OK)
    {
        return st;
    }

    uint8_t buf[6];
    st = pn5180_pack_reg_write(PN5180_CMD_WRITE_REGISTER_AND_MASK, reg, mask, buf);
    if (st != PN5180_OK)
    {
        return st;
    }
    return pn5180_transceive(dev, buf, 6u, NULL, 0u);
}

pn5180_status_t pn5180_read_register(pn5180_t *dev, uint8_t reg, uint32_t *value)
{
    pn5180_status_t st = pn5180_check_callbacks(dev);
    if (st != PN5180_OK)
    {
        return st;
    }
    if (value == NULL)
    {
        return PN5180_ERROR_PARAM;
    }

    uint8_t cmd[2] = { PN5180_CMD_READ_REGISTER, reg };
    uint8_t rx[4];

    st = pn5180_transceive(dev, cmd, 2u, rx, 4u);
    if (st != PN5180_OK)
    {
        return st;
    }

    *value = ((uint32_t)rx[0]) | ((uint32_t)rx[1] << 8) | ((uint32_t)rx[2] << 16) |
             ((uint32_t)rx[3] << 24);
    return PN5180_OK;
}

pn5180_status_t pn5180_write_eeprom(pn5180_t *dev, uint8_t addr, const uint8_t *data, uint8_t len)
{
    pn5180_status_t st = pn5180_check_ready(dev);
    if (st != PN5180_OK)
    {
        return st;
    }
    if (data == NULL || len == 0u)
    {
        return PN5180_ERROR_PARAM;
    }
    if (addr > 254u || ((uint16_t)addr + (uint16_t)len) > 255u)
    {
        return PN5180_ERROR_PARAM;
    }

    uint8_t buf[2u + 255u];
    buf[0] = PN5180_CMD_WRITE_EEPROM;
    buf[1] = addr;
    memcpy(&buf[2], data, len);

    return pn5180_transceive(dev, buf, (uint16_t)(2u + len), NULL, 0u);
}

pn5180_status_t pn5180_read_eeprom(pn5180_t *dev, uint8_t addr, uint8_t *data, uint8_t len)
{
    pn5180_status_t st = pn5180_check_ready(dev);
    if (st != PN5180_OK)
    {
        return st;
    }
    return pn5180_read_eeprom_raw(dev, addr, data, len);
}

static pn5180_status_t pn5180_read_eeprom_raw(pn5180_t *dev, uint8_t addr, uint8_t *data, uint8_t len)
{
    if (data == NULL || len == 0u)
    {
        return PN5180_ERROR_PARAM;
    }
    if (addr > 254u || ((uint16_t)addr + (uint16_t)len) > 255u)
    {
        return PN5180_ERROR_PARAM;
    }

    uint8_t cmd[3] = { PN5180_CMD_READ_EEPROM, addr, len };
    return pn5180_transceive(dev, cmd, 3u, data, len);
}

pn5180_status_t pn5180_load_rf_config(pn5180_t *dev, uint8_t tx_conf, uint8_t rx_conf)
{
    pn5180_status_t st = pn5180_check_ready(dev);
    if (st != PN5180_OK)
    {
        return st;
    }

    uint8_t cmd[3] = { PN5180_CMD_LOAD_RF_CONFIG, tx_conf, rx_conf };
    return pn5180_transceive(dev, cmd, 3u, NULL, 0u);
}

pn5180_status_t pn5180_rf_on(pn5180_t *dev)
{
    pn5180_status_t st = pn5180_check_ready(dev);
    if (st != PN5180_OK)
    {
        return st;
    }

    uint8_t cmd[2] = { PN5180_CMD_RF_ON, 0x00u };
    st = pn5180_transceive(dev, cmd, 2u, NULL, 0u);
    if (st != PN5180_OK)
    {
        return st;
    }

    st = pn5180_wait_irq(dev, PN5180_IRQ_TX_RFON_STAT, dev->busy_timeout_ms);
    if (st != PN5180_OK)
    {
        return st;
    }
    return pn5180_clear_irq_status(dev, PN5180_IRQ_TX_RFON_STAT);
}

pn5180_status_t pn5180_rf_off(pn5180_t *dev)
{
    pn5180_status_t st = pn5180_check_ready(dev);
    if (st != PN5180_OK)
    {
        return st;
    }

    uint8_t cmd[2] = { PN5180_CMD_RF_OFF, 0x00u };
    st = pn5180_transceive(dev, cmd, 2u, NULL, 0u);
    if (st != PN5180_OK)
    {
        return st;
    }

    st = pn5180_wait_irq(dev, PN5180_IRQ_TX_RFOFF_STAT, dev->busy_timeout_ms);
    if (st != PN5180_OK)
    {
        return st;
    }
    return pn5180_clear_irq_status(dev, PN5180_IRQ_TX_RFOFF_STAT);
}

pn5180_status_t pn5180_setup_rf(pn5180_t *dev, uint8_t tx_conf, uint8_t rx_conf)
{
    pn5180_status_t st = pn5180_check_ready(dev);
    if (st != PN5180_OK)
    {
        return st;
    }

    st = pn5180_load_rf_config(dev, tx_conf, rx_conf);
    if (st != PN5180_OK)
    {
        return st;
    }

    st = pn5180_rf_on(dev);
    if (st != PN5180_OK)
    {
        return st;
    }

    st = pn5180_write_register_and_mask(dev, PN5180_REG_SYSTEM_CONFIG, 0xFFFFFFF8u);
    if (st != PN5180_OK)
    {
        return st;
    }

    return pn5180_write_register_or_mask(dev, PN5180_REG_SYSTEM_CONFIG, PN5180_SYS_CMD_TRANSCEIVE);
}

pn5180_status_t pn5180_send_data(pn5180_t *dev, const uint8_t *data, uint16_t len, uint8_t valid_bits)
{
    pn5180_status_t st = pn5180_check_ready(dev);
    if (st != PN5180_OK)
    {
        return st;
    }
    if (data == NULL || len > PN5180_MAX_TX_PAYLOAD || valid_bits > 7u)
    {
        return PN5180_ERROR_PARAM;
    }

    uint8_t buf[2u + PN5180_MAX_TX_PAYLOAD];
    buf[0] = PN5180_CMD_SEND_DATA;
    buf[1] = valid_bits;
    memcpy(&buf[2], data, len);

    return pn5180_transceive(dev, buf, (uint16_t)(2u + len), NULL, 0u);
}

pn5180_status_t pn5180_read_data(pn5180_t *dev, uint8_t *buf, uint16_t len)
{
    pn5180_status_t st = pn5180_check_ready(dev);
    if (st != PN5180_OK)
    {
        return st;
    }
    if (buf == NULL || len == 0u || len > PN5180_MAX_RX_PAYLOAD)
    {
        return PN5180_ERROR_PARAM;
    }

    uint8_t cmd[2] = { PN5180_CMD_READ_DATA, 0x00u };
    return pn5180_transceive(dev, cmd, 2u, buf, len);
}

pn5180_status_t pn5180_get_irq_status(pn5180_t *dev, uint32_t *status)
{
    if (status == NULL)
    {
        return PN5180_ERROR_PARAM;
    }
    return pn5180_read_register(dev, PN5180_REG_IRQ_STATUS, status);
}

pn5180_status_t pn5180_clear_irq_status(pn5180_t *dev, uint32_t mask)
{
    pn5180_status_t st = pn5180_check_callbacks(dev);
    if (st != PN5180_OK)
    {
        return st;
    }
    return pn5180_write_register(dev, PN5180_REG_IRQ_CLEAR, mask);
}

pn5180_status_t pn5180_get_transceive_state(pn5180_t *dev, uint8_t *state)
{
    pn5180_status_t st = pn5180_check_ready(dev);
    if (st != PN5180_OK)
    {
        return st;
    }
    if (state == NULL)
    {
        return PN5180_ERROR_PARAM;
    }

    uint32_t rf_status = 0u;
    st = pn5180_read_register(dev, PN5180_REG_RF_STATUS, &rf_status);
    if (st != PN5180_OK)
    {
        return st;
    }

    *state = (uint8_t)((rf_status >> 24) & 0x07u);
    return PN5180_OK;
}

pn5180_status_t pn5180_get_rx_length(pn5180_t *dev, uint16_t *len)
{
    pn5180_status_t st = pn5180_check_ready(dev);
    if (st != PN5180_OK)
    {
        return st;
    }
    if (len == NULL)
    {
        return PN5180_ERROR_PARAM;
    }

    uint32_t rx_status = 0u;
    st = pn5180_read_register(dev, PN5180_REG_RX_STATUS, &rx_status);
    if (st != PN5180_OK)
    {
        return st;
    }

    *len = (uint16_t)(rx_status & 0x01FFu);
    return PN5180_OK;
}

pn5180_status_t pn5180_wait_irq(pn5180_t *dev, uint32_t mask, uint32_t timeout_ms)
{
    if (mask == 0u)
    {
        return PN5180_ERROR_PARAM;
    }
    if (timeout_ms == 0u)
    {
        timeout_ms = dev->busy_timeout_ms;
    }
    if (timeout_ms == 0u)
    {
        timeout_ms = 300u;
    }

    uint32_t elapsed = 0u;
    const uint32_t poll_ms = 10u;

    while (elapsed < timeout_ms)
    {
        uint32_t status = 0u;
        pn5180_status_t st = pn5180_get_irq_status(dev, &status);
        if (st != PN5180_OK)
        {
            return st;
        }
        if ((status & mask) != 0u)
        {
            return PN5180_OK;
        }
        if (dev->delay_ms == NULL)
        {
            return PN5180_ERROR_TIMEOUT;
        }
        dev->delay_ms(poll_ms);
        elapsed += poll_ms;
    }
    return PN5180_ERROR_TIMEOUT;
}

pn5180_status_t pn5180_read_firmware_version(pn5180_t *dev, uint8_t version[2])
{
    pn5180_status_t st = pn5180_check_callbacks(dev);
    if (st != PN5180_OK)
    {
        return st;
    }
    if (version == NULL)
    {
        return PN5180_ERROR_PARAM;
    }
    return pn5180_read_eeprom_raw(dev, PN5180_EEPROM_FIRMWARE_VERSION, version, 2u);
}

bool pn5180_is_initialized(const pn5180_t *dev)
{
    return (dev != NULL) && dev->initialized;
}
