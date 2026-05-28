/**
 * @file    zb25vq16.h
 * @brief   ZB25VQ16ATIG (16Mbit / 2MB) SPI NOR — W25Q-class command set
 *
 * Chip layer only; SPI hooks via zb25vq16_port.h (board / ext_flash).
 * Phase 1 fills read/program/erase; phase 0 provides types and init stub.
 */

#ifndef ZB25VQ16_H
#define ZB25VQ16_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZB25VQ16_FLASH_SIZE         (2U * 1024U * 1024U)
#define ZB25VQ16_SECTOR_SIZE        0x1000U
#define ZB25VQ16_PAGE_SIZE          256U
#define ZB25VQ16_SECTOR_COUNT       (ZB25VQ16_FLASH_SIZE / ZB25VQ16_SECTOR_SIZE)

/** Expected JEDEC capacity byte (16Mbit); manufacturer may be 0xEF or 0x5E */
#define ZB25VQ16_JEDEC_MANUFACTURER_WINBOND  0xEFU
#define ZB25VQ16_JEDEC_MANUFACTURER_ZBIT     0x5EU
#define ZB25VQ16_JEDEC_MEMORY                0x40U
#define ZB25VQ16_JEDEC_CAPACITY              0x15U

/* Standard SPI commands */
#define ZB25_CMD_READ_DATA          0x03U
#define ZB25_CMD_PAGE_PROGRAM       0x02U
#define ZB25_CMD_SECTOR_ERASE       0x20U
#define ZB25_CMD_CHIP_ERASE         0xC7U
#define ZB25_CMD_READ_STATUS1       0x05U
#define ZB25_CMD_WRITE_ENABLE       0x06U
#define ZB25_CMD_JEDEC_ID           0x9FU
#define ZB25_CMD_RELEASE_POWERDOWN  0xABU

typedef enum {
    ZB25_OK = 0,
    ZB25_ERR_PARAM,
    ZB25_ERR_NOT_INIT,
    ZB25_ERR_SPI,
    ZB25_ERR_TIMEOUT,
    ZB25_ERR_ID,
    ZB25_ERR_WEL
} zb25vq16_status_t;

zb25vq16_status_t zb25vq16_init(void);
zb25vq16_status_t zb25vq16_read_jedec_id(uint8_t id[3]);
zb25vq16_status_t zb25vq16_read(uint32_t addr, uint8_t *buf, uint32_t len);
zb25vq16_status_t zb25vq16_write(uint32_t addr, const uint8_t *buf, uint32_t len);
zb25vq16_status_t zb25vq16_erase_sector(uint32_t sector_index);
bool zb25vq16_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif /* ZB25VQ16_H */
