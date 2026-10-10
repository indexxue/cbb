/**
 * @file    w25q.h
 * @brief   Winbond W25Q-family SPI NOR (standard SPI, callback-injected, no MCU deps).
 *
 * JEDEC capacity 0x14..0x18 maps to 1MB..16MB (3-byte address). Compatible IDs
 * include Winbond 0xEF and Zbit 0x5E. Dual/Quad SPI and 4-byte address are out of scope.
 */

#ifndef W25Q_H
#define W25Q_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define W25Q_PAGE_SIZE          256U
#define W25Q_SECTOR_SIZE        0x1000U

#define W25Q_JEDEC_MANUF_WINBOND  0xEFU
#define W25Q_JEDEC_MANUF_ZBIT     0x5EU
#define W25Q_JEDEC_CAPACITY_MIN   0x14U
#define W25Q_JEDEC_CAPACITY_MAX   0x18U

#define W25Q_CMD_WRITE_STATUS1      0x01U
#define W25Q_CMD_PAGE_PROGRAM       0x02U
#define W25Q_CMD_READ_DATA          0x03U
#define W25Q_CMD_READ_STATUS1       0x05U
#define W25Q_CMD_WRITE_ENABLE       0x06U
#define W25Q_CMD_SECTOR_ERASE       0x20U
#define W25Q_CMD_JEDEC_ID           0x9FU
#define W25Q_CMD_RELEASE_POWERDOWN  0xABU

/**
 * @brief Full-duplex SPI transfer while CS is held by the driver.
 * @param rx May be NULL when only TX is required.
 * @return 0 on success, non-zero on failure.
 */
typedef int (*w25q_spi_xfer_t)(const uint8_t *tx, uint8_t *rx, uint16_t len);

/** @brief Chip select: 0 = selected (low), 1 = released. */
typedef void (*w25q_set_cs_t)(uint8_t level);

typedef void (*w25q_delay_ms_t)(uint32_t ms);

typedef uint32_t (*w25q_get_tick_ms_t)(void);

/**
 * @brief Optional streaming RX after a read command header (CS already low).
 * @return 0 on success, non-zero on failure.
 */
typedef int (*w25q_read_stream_t)(uint8_t *buf, uint32_t len);

typedef enum {
    W25Q_OK = 0,
    W25Q_ERR_PARAM,
    W25Q_ERR_NOT_INIT,
    W25Q_ERR_BUS,
    W25Q_ERR_TIMEOUT,
    W25Q_ERR_ID,
    W25Q_ERR_WEL
} w25q_status_t;

typedef struct {
    w25q_spi_xfer_t     spi_xfer;
    w25q_set_cs_t       set_cs;
    w25q_delay_ms_t     delay_ms;
    w25q_get_tick_ms_t  get_tick_ms;
    w25q_read_stream_t  read_stream;    /**< Optional; NULL uses spi_xfer chunks. */
    bool                skip_id_check;  /**< Factory: skip manufacturer whitelist. */
} w25q_config_t;

typedef struct {
    w25q_spi_xfer_t     spi_xfer;
    w25q_set_cs_t       set_cs;
    w25q_delay_ms_t     delay_ms;
    w25q_get_tick_ms_t  get_tick_ms;
    w25q_read_stream_t  read_stream;
    bool                skip_id_check;
    uint8_t             jedec[3];
    uint32_t            flash_size;
    uint32_t            sector_count;
    bool                initialized;
} w25q_t;

/** @brief Bind callbacks, wake chip, verify JEDEC, clear BP, mark initialized. */
w25q_status_t w25q_init(w25q_t *dev, const w25q_config_t *cfg);

/**
 * @brief Read JEDEC ID (0x9F). Requires spi_xfer/set_cs on @p dev; init not required.
 */
w25q_status_t w25q_read_jedec_id(w25q_t *dev, uint8_t id[3]);

/** @brief Flash size in bytes after successful init; else 0. */
uint32_t w25q_get_size(const w25q_t *dev);

/** @brief Sector count (4 KiB) after successful init; else 0. */
uint32_t w25q_get_sector_count(const w25q_t *dev);

w25q_status_t w25q_read(w25q_t *dev, uint32_t addr, uint8_t *buf, uint32_t len);
w25q_status_t w25q_write(w25q_t *dev, uint32_t addr, const uint8_t *buf, uint32_t len);
w25q_status_t w25q_erase_sector(w25q_t *dev, uint32_t sector_index);
bool w25q_is_initialized(const w25q_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* W25Q_H */
