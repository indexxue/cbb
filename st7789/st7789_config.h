/**
 * @file    st7789_config.h
 * @brief   RAM/吞吐参数（可在编译前 #define 覆盖，适配 STM32 32KB SRAM 等）
 */

#ifndef ST7789_CONFIG_H
#define ST7789_CONFIG_H

/** `st7789_write_pixels` 展开缓冲；默认 1KB（ESP 侧可在工程里改为 4096） */
#ifndef ST7789_TX_CHUNK
#define ST7789_TX_CHUNK         512U
#endif

/** `lcd_fill` 行缓冲，逻辑宽最大 240 */
#ifndef LCD_LINEBUF_MAX
#define LCD_LINEBUF_MAX         240U
#endif

/** `lcd_fill_fast` 批量写显存块；默认 2KB（原 ESP 参考为 32KB） */
#ifndef LCD_FAST_FILL_BLK
/** 至少容纳一行 RGB565（240x2）；再小则 `lcd_fill_fast` 退化为逐行 */
#define LCD_FAST_FILL_BLK       512U
#endif

#endif /* ST7789_CONFIG_H */
