/**
 * @file    zb25vq16_port.h
 * @brief   Board hooks for ZB25VQ16 SPI (implemented in ext_flash.c)
 */

#ifndef ZB25VQ16_PORT_H
#define ZB25VQ16_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZB25VQ16_PORT_DMA_READ_MIN      256U

void zb25vq16_port_cs_low(void);
void zb25vq16_port_cs_high(void);

/** Full-duplex transfer while CS is held by caller */
int zb25vq16_port_xfer(const uint8_t *tx, uint8_t *rx, uint16_t len);

/**
 * Clock in @p len bytes after read command header (CS already low).
 * Uses DMA when @p len >= ZB25VQ16_PORT_DMA_READ_MIN.
 */
int zb25vq16_port_read_stream(uint8_t *buf, uint32_t len);

void zb25vq16_port_delay_ms(uint32_t ms);

uint32_t zb25vq16_port_get_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* ZB25VQ16_PORT_H */
