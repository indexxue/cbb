/**
 * @file    pn5180.h
 * @brief   NXP PN5180 多协议 NFC 前端（SPI + BUSY），MCU 无关通用驱动。
 *
 * 通过回调注入 SPI、NSS、BUSY、RST、天线电源与延时，实现主机接口 Direct Command、
 * 寄存器/EEPROM 访问及 RF 场控制。不包含 ISO15693 / ISO14443 等应用层协议。
 *
 * @see NXP PN5180A0XX/C3,C4 datasheet
 * @see doc/io_pin_assignment.md（本工程 SPI1 引脚）
 */

#ifndef PN5180_H
#define PN5180_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* Host interface direct command codes (1 byte)                                 */
/* -------------------------------------------------------------------------- */

#define PN5180_CMD_WRITE_REGISTER         0x00u
#define PN5180_CMD_WRITE_REGISTER_OR_MASK 0x01u
#define PN5180_CMD_WRITE_REGISTER_AND_MASK 0x02u
#define PN5180_CMD_READ_REGISTER          0x04u
#define PN5180_CMD_WRITE_EEPROM           0x06u
#define PN5180_CMD_READ_EEPROM            0x07u
#define PN5180_CMD_SEND_DATA              0x09u
#define PN5180_CMD_READ_DATA              0x0Au
#define PN5180_CMD_LOAD_RF_CONFIG         0x11u
#define PN5180_CMD_RF_ON                  0x16u
#define PN5180_CMD_RF_OFF                 0x17u

/* -------------------------------------------------------------------------- */
/* Register addresses                                                         */
/* -------------------------------------------------------------------------- */

#define PN5180_REG_SYSTEM_CONFIG      0x00u
#define PN5180_REG_IRQ_ENABLE         0x01u
#define PN5180_REG_IRQ_STATUS         0x02u
#define PN5180_REG_IRQ_CLEAR          0x03u
#define PN5180_REG_TRANSCEIVE_CONTROL 0x04u
#define PN5180_REG_TIMER1_RELOAD      0x0Cu
#define PN5180_REG_TIMER1_CONFIG      0x0Fu
#define PN5180_REG_RX_WAIT_CONFIG     0x11u
#define PN5180_REG_CRC_RX_CONFIG      0x12u
#define PN5180_REG_RX_STATUS           0x13u
#define PN5180_REG_CRC_TX_CONFIG      0x19u
#define PN5180_REG_RF_STATUS           0x1Du
#define PN5180_REG_SYSTEM_STATUS       0x24u
#define PN5180_REG_TEMP_CONTROL        0x25u

/* -------------------------------------------------------------------------- */
/* EEPROM addresses                                                           */
/* -------------------------------------------------------------------------- */

#define PN5180_EEPROM_DIE_IDENTIFIER   0x00u
#define PN5180_EEPROM_PRODUCT_VERSION  0x10u
#define PN5180_EEPROM_FIRMWARE_VERSION 0x12u
#define PN5180_EEPROM_VERSION          0x14u
#define PN5180_EEPROM_IRQ_PIN_CONFIG   0x1Au
#define PN5180_EEPROM_SIZE             255u

/* -------------------------------------------------------------------------- */
/* IRQ_STATUS bits (register 0x02)                                            */
/* -------------------------------------------------------------------------- */

#define PN5180_IRQ_RX_STAT           (1u << 0)
#define PN5180_IRQ_TX_STAT           (1u << 1)
#define PN5180_IRQ_IDLE_STAT         (1u << 2)
#define PN5180_IRQ_RFOFF_DET_STAT    (1u << 6)
#define PN5180_IRQ_RFON_DET_STAT     (1u << 7)
#define PN5180_IRQ_TX_RFOFF_STAT     (1u << 8)
#define PN5180_IRQ_TX_RFON_STAT      (1u << 9)
#define PN5180_IRQ_RX_SOF_DET_STAT   (1u << 14)
#define PN5180_IRQ_GENERAL_ERROR     (1u << 15)

/* SYSTEM_CONFIG.COMMAND field */
#define PN5180_SYS_CMD_IDLE          0x00u
#define PN5180_SYS_CMD_TRANSCEIVE    0x03u

/* RF_STATUS.TRANSCEIVE_STATE (bits 26:24) */
#define PN5180_TS_IDLE               0u
#define PN5180_TS_WAIT_TRANSMIT      1u
#define PN5180_TS_TRANSMITTING       2u
#define PN5180_TS_WAIT_RECEIVE       3u
#define PN5180_TS_WAIT_FOR_DATA      4u
#define PN5180_TS_RECEIVING          5u

/** ISO15693 26 kHz ASK100 TX / 26 kHz RX（NXP EEPROM 预设 0x0D / 0x8D） */
#define PN5180_RF_TX_ISO15693_26     0x0Du
#define PN5180_RF_RX_ISO15693_26     0x8Du

/** ISO14443-A 106 kbps Miller TX / Manch SubC RX（NXP 预设 0x00 / 0x80） */
#define PN5180_RF_TX_ISO14443A       0x00u
#define PN5180_RF_RX_ISO14443A       0x80u

#define PN5180_MAX_TX_PAYLOAD        260u
#define PN5180_MAX_RX_PAYLOAD        508u

/**
 * SPI 全双工交换：`tx`/`rx` 长度均为 `len`；仅发送时 `rx` 可为 NULL。
 * @return 0 成功，非 0 失败。
 */
typedef int (*pn5180_spi_tx_rx_t)(const uint8_t *tx, uint8_t *rx, uint16_t len);

/** @param high 非 0 = 高电平。NSS 低有效，RST 低有效。 */
typedef void (*pn5180_pin_out_t)(int high);

/** @return 0 或 1（非 0 视为高）。 */
typedef int (*pn5180_pin_in_t)(void);

typedef void (*pn5180_delay_ms_t)(uint32_t ms);

typedef enum {
    PN5180_OK = 0,
    PN5180_ERROR_PARAM,
    PN5180_ERROR_NOT_INIT,
    PN5180_ERROR_BUS,
    PN5180_ERROR_BUSY,
    PN5180_ERROR_TIMEOUT,
    PN5180_ERROR_ID,
} pn5180_status_t;

typedef struct {
    pn5180_spi_tx_rx_t spi_tx_rx;
    pn5180_pin_out_t   set_nss;
    pn5180_pin_in_t    read_busy;
    pn5180_pin_out_t   set_rst;
    /** 可选：天线电源使能；NULL 则 `pn5180_init` 不操作电源脚。 */
    pn5180_pin_out_t   set_pwr_on;
    pn5180_delay_ms_t  delay_ms;
    /** 等待 BUSY 释放的最长时间（ms）；0 = 不超时。 */
    uint32_t           busy_timeout_ms;
    /** true：BUSY 高 = 忙（NXP 默认）。 */
    bool               busy_active_high;
    /** true：`pn5180_init` 内执行硬件复位并校验 EEPROM 版本。 */
    bool               reset_on_init;
} pn5180_config_t;

#define PN5180_CONFIG_DEFAULT                       \
    {                                               \
        .spi_tx_rx        = NULL,                   \
        .set_nss          = NULL,                   \
        .read_busy        = NULL,                   \
        .set_rst          = NULL,                   \
        .set_pwr_on       = NULL,                   \
        .delay_ms         = NULL,                   \
        .busy_timeout_ms  = 1000u,                  \
        .busy_active_high = true,                   \
        .reset_on_init    = true,                   \
    }

typedef struct {
    pn5180_spi_tx_rx_t spi_tx_rx;
    pn5180_pin_out_t   set_nss;
    pn5180_pin_in_t    read_busy;
    pn5180_pin_out_t   set_rst;
    pn5180_pin_out_t   set_pwr_on;
    pn5180_delay_ms_t  delay_ms;
    uint32_t           busy_timeout_ms;
    bool               busy_active_high;
    bool               reset_on_init;
    bool               initialized;
} pn5180_t;

pn5180_status_t pn5180_register(pn5180_t *dev, const pn5180_config_t *cfg);

/**
 * 注册回调；若 `reset_on_init` 为 true 则上电/复位并读 Firmware Version 校验通信。
 */
pn5180_status_t pn5180_init(pn5180_t *dev);

/** 低有效复位脉冲（≥ 10 µs 低，≥ 2 ms 恢复），并等待 IDLE 中断。 */
pn5180_status_t pn5180_reset(pn5180_t *dev);

pn5180_status_t pn5180_write_register(pn5180_t *dev, uint8_t reg, uint32_t value);
pn5180_status_t pn5180_write_register_or_mask(pn5180_t *dev, uint8_t reg, uint32_t mask);
pn5180_status_t pn5180_write_register_and_mask(pn5180_t *dev, uint8_t reg, uint32_t mask);
pn5180_status_t pn5180_read_register(pn5180_t *dev, uint8_t reg, uint32_t *value);

pn5180_status_t pn5180_write_eeprom(pn5180_t *dev, uint8_t addr, const uint8_t *data, uint8_t len);
pn5180_status_t pn5180_read_eeprom(pn5180_t *dev, uint8_t addr, uint8_t *data, uint8_t len);

pn5180_status_t pn5180_load_rf_config(pn5180_t *dev, uint8_t tx_conf, uint8_t rx_conf);
pn5180_status_t pn5180_rf_on(pn5180_t *dev);
pn5180_status_t pn5180_rf_off(pn5180_t *dev);

/**
 * 加载 RF 配置、打开 RF 场，并将 SYSTEM_CONFIG 设为 Transceive 模式。
 */
pn5180_status_t pn5180_setup_rf(pn5180_t *dev, uint8_t tx_conf, uint8_t rx_conf);

/**
 * 写 TX 缓冲并启动发送；调用前需已 `pn5180_setup_rf` 或手动进入 WaitTransmit。
 * @param valid_bits 最后一字节有效位数（0–7，0 = 整字节）。
 */
pn5180_status_t pn5180_send_data(pn5180_t *dev, const uint8_t *data, uint16_t len, uint8_t valid_bits);

/** 从 RX 缓冲读取 `len` 字节到 `buf`（调用方提供缓冲）。 */
pn5180_status_t pn5180_read_data(pn5180_t *dev, uint8_t *buf, uint16_t len);

pn5180_status_t pn5180_get_irq_status(pn5180_t *dev, uint32_t *status);
pn5180_status_t pn5180_clear_irq_status(pn5180_t *dev, uint32_t mask);

/** 读取 RF_STATUS 中的 TRANSCEIVE_STATE（0–7）。 */
pn5180_status_t pn5180_get_transceive_state(pn5180_t *dev, uint8_t *state);

/** 从 RX_STATUS 低 9 位取最近一次接收字节数。 */
pn5180_status_t pn5180_get_rx_length(pn5180_t *dev, uint16_t *len);

/**
 * 阻塞等待 IRQ_STATUS 中 `mask` 位置位。
 * @param timeout_ms 0 = 使用 dev->busy_timeout_ms。
 */
pn5180_status_t pn5180_wait_irq(pn5180_t *dev, uint32_t mask, uint32_t timeout_ms);

pn5180_status_t pn5180_read_firmware_version(pn5180_t *dev, uint8_t version[2]);

bool pn5180_is_initialized(const pn5180_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* PN5180_H */
