/**
 * @file    epd_2in9.h
 * @brief   2.9 寸黑白电子墨水屏（128×296）通用驱动，回调 SPI/GPIO，MCU 无关。
 *
 * 兼容 Waveshare 2.9inch e-Paper Module：
 * - `EPD_2IN9_VARIANT_V1`：SSD1606（旧版）
 * - `EPD_2IN9_VARIANT_V2`：SSD1680（V2，默认）
 *
 * 帧缓冲：每字节 8 像素，MSB 先出；位 1 = 白，位 0 = 黑；行优先，共 `EPD_2IN9_FRAME_SIZE` 字节。
 */

#ifndef EPD_2IN9_H
#define EPD_2IN9_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EPD_2IN9_WIDTH           128U
#define EPD_2IN9_HEIGHT          296U
#define EPD_2IN9_BYTES_PER_ROW   ((EPD_2IN9_WIDTH + 7U) / 8U)
#define EPD_2IN9_FRAME_SIZE      (EPD_2IN9_BYTES_PER_ROW * EPD_2IN9_HEIGHT)

typedef void (*epd_2in9_spi_tx_t)(const uint8_t *data, uint16_t len);
typedef void (*epd_2in9_pin_out_t)(int high);
/** @return 引脚电平：0 或 1（非 0 视为高）。 */
typedef int (*epd_2in9_pin_in_t)(void);
typedef void (*epd_2in9_delay_ms_t)(uint32_t ms);
/** 按行填充显示数据，`row` 长度 `EPD_2IN9_BYTES_PER_ROW`。 */
typedef void (*epd_2in9_fill_row_t)(uint16_t y, uint8_t *row, void *ctx);

typedef enum {
    EPD_2IN9_OK = 0,
    EPD_2IN9_ERROR_PARAM,
    EPD_2IN9_ERROR_NOT_INIT,
    EPD_2IN9_ERROR_VARIANT,
    EPD_2IN9_ERROR_BUSY,
} epd_2in9_status_t;

typedef enum {
    EPD_2IN9_VARIANT_V1 = 0,
    EPD_2IN9_VARIANT_V2 = 1,
} epd_2in9_variant_t;

/** V1 初始化时的 LUT 模式；V2 忽略。 */
typedef enum {
    EPD_2IN9_LUT_FULL = 0,
    EPD_2IN9_LUT_PARTIAL = 1,
} epd_2in9_lut_mode_t;

typedef struct {
    epd_2in9_spi_tx_t spi_tx;
    epd_2in9_pin_out_t set_cs;
    epd_2in9_pin_out_t set_dc;
    epd_2in9_pin_out_t set_rst;
    epd_2in9_pin_in_t read_busy;
    epd_2in9_delay_ms_t delay_ms;
    epd_2in9_variant_t variant;
    /** true：BUSY 为高电平表示忙（Waveshare 默认）。 */
    bool busy_active_high;
    /** 仅 V1 `epd_2in9_init` 使用。 */
    epd_2in9_lut_mode_t lut_mode;
} epd_2in9_config_t;

typedef struct {
    epd_2in9_spi_tx_t spi_tx;
    epd_2in9_pin_out_t set_cs;
    epd_2in9_pin_out_t set_dc;
    epd_2in9_pin_out_t set_rst;
    epd_2in9_pin_in_t read_busy;
    epd_2in9_delay_ms_t delay_ms;
    epd_2in9_variant_t variant;
    bool busy_active_high;
    epd_2in9_lut_mode_t lut_mode;
    bool initialized;
} epd_2in9_t;

epd_2in9_status_t epd_2in9_register(epd_2in9_t *dev, const epd_2in9_config_t *cfg);

/** 标准全刷初始化（V2 使用 WS_20_30 LUT；V1 使用 lut_mode）。 */
epd_2in9_status_t epd_2in9_init(epd_2in9_t *dev);

/** V2 快速全刷初始化；V1 返回 `EPD_2IN9_ERROR_VARIANT`。 */
epd_2in9_status_t epd_2in9_init_fast(epd_2in9_t *dev);

/** 刷白屏并全刷更新。 */
epd_2in9_status_t epd_2in9_clear(epd_2in9_t *dev);

/**
 * 显示黑白帧缓冲并全刷。
 * @param image 长度至少 `EPD_2IN9_FRAME_SIZE`；V2 仅写 0x24 RAM。
 */
epd_2in9_status_t epd_2in9_display(epd_2in9_t *dev, const uint8_t *image);

/**
 * 按行生成并全刷显示，无需整帧 RAM（适合 STM32F072 等小 SRAM MCU）。
 */
epd_2in9_status_t epd_2in9_display_stream(epd_2in9_t *dev, epd_2in9_fill_row_t fill_row, void *ctx);

/**
 * V2：双 RAM（0x24 + 0x26）全刷，利于后续局部刷新基图。
 * V1 等价于 `epd_2in9_display`。
 */
epd_2in9_status_t epd_2in9_display_base(epd_2in9_t *dev, const uint8_t *image);

/** V2 局部刷新；V1 返回 `EPD_2IN9_ERROR_VARIANT`。 */
epd_2in9_status_t epd_2in9_display_partial(epd_2in9_t *dev, const uint8_t *image);

/** 进入深度睡眠（需重新 init 后才能再次刷新）。 */
epd_2in9_status_t epd_2in9_sleep(epd_2in9_t *dev);

/** 阻塞等待 BUSY 释放。 */
epd_2in9_status_t epd_2in9_wait_idle(epd_2in9_t *dev);

bool epd_2in9_is_initialized(const epd_2in9_t *dev);
epd_2in9_variant_t epd_2in9_get_variant(const epd_2in9_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* EPD_2IN9_H */
