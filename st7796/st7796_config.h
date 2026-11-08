/**
 * @file    st7796_config.h
 * @brief   RAM/吞吐参数（可在编译前 #define 覆盖）
 */

#ifndef ST7796_CONFIG_H
#define ST7796_CONFIG_H

/** `st7796_write_pixels` 展开缓冲；默认 512 字节（256 像素 RGB565） */
#ifndef ST7796_TX_CHUNK
#define ST7796_TX_CHUNK 512U
#endif

#endif /* ST7796_CONFIG_H */
