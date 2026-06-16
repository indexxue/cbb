/**
 * @file    epd_2in9b.h
 * @brief   2.9 寸红白黑电子墨水屏（128×296，SSD1680A）通用驱动。
 *
 * 帧缓冲为两个 1bpp 平面，各 `EPD_2IN9B_PLANE_SIZE` 字节：
 * - 黑白平面：位 1 = 白，位 0 = 黑（逻辑帧缓冲；写 RAM 0x24 时可按 `bw_invert` 取反）
 * - 红色平面：位 1 = 红，位 0 = 非红（写 RAM 0x26，驱动内部取反）
 */

#ifndef EPD_2IN9B_H
#define EPD_2IN9B_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EPD_2IN9B_WIDTH           128U
#define EPD_2IN9B_HEIGHT          296U
#define EPD_2IN9B_BYTES_PER_ROW   ((EPD_2IN9B_WIDTH + 7U) / 8U)
#define EPD_2IN9B_PLANE_SIZE      (EPD_2IN9B_BYTES_PER_ROW * EPD_2IN9B_HEIGHT)

typedef void (*epd_2in9b_spi_tx_t)(const uint8_t *data, uint16_t len);
typedef void (*epd_2in9b_pin_out_t)(int high);
typedef int (*epd_2in9b_pin_in_t)(void);
typedef void (*epd_2in9b_delay_ms_t)(uint32_t ms);

typedef enum {
    EPD_2IN9B_OK = 0,
    EPD_2IN9B_ERROR_PARAM,
    EPD_2IN9B_ERROR_NOT_INIT,
    EPD_2IN9B_ERROR_BUSY,
} epd_2in9b_status_t;

typedef struct {
    epd_2in9b_spi_tx_t spi_tx;
    epd_2in9b_pin_out_t set_cs;
    epd_2in9b_pin_out_t set_dc;
    epd_2in9b_pin_out_t set_rst;
    epd_2in9b_pin_in_t read_busy;
    epd_2in9b_delay_ms_t delay_ms;
    /** true：BUSY 为高电平表示忙（Waveshare / SSD1680A 默认）。 */
    bool busy_active_high;
    /**
     * true：写 RAM 0x24 时对黑白平面按位取反。
     * 部分 S-GDEY029Z95 模组与官方 demo 源图极性相反（0xFF 刷出全黑）。
     */
    bool bw_invert;
} epd_2in9b_config_t;

typedef struct {
    epd_2in9b_spi_tx_t spi_tx;
    epd_2in9b_pin_out_t set_cs;
    epd_2in9b_pin_out_t set_dc;
    epd_2in9b_pin_out_t set_rst;
    epd_2in9b_pin_in_t read_busy;
    epd_2in9b_delay_ms_t delay_ms;
    bool busy_active_high;
    bool bw_invert;
    bool initialized;
} epd_2in9b_t;

epd_2in9b_status_t epd_2in9b_register(epd_2in9b_t *dev, const epd_2in9b_config_t *cfg);
epd_2in9b_status_t epd_2in9b_init(epd_2in9b_t *dev);
epd_2in9b_status_t epd_2in9b_clear(epd_2in9b_t *dev);
/**
 * 仅写 RAM 0x24 并全刷（GoodDisplay EPD_WhiteScreen_ALL / _Black）。
 * 红色层保持模组内上次内容；纯黑白测试用此接口。
 */
epd_2in9b_status_t epd_2in9b_display_mono(epd_2in9b_t *dev, const uint8_t *bw_plane);
/**
 * 写 RAM 0x24 + 0x26 并全刷（GoodDisplay EPD_SetRAMValue_BaseMap 三色流程）。
 * @param bw_plane  黑白平面，位 1=白、0=黑；写 0x24 时按 `bw_invert` 取反。
 * @param red_plane 红色平面，位 1=红、0=非红；写 0x26 时驱动内部取反。
 */
epd_2in9b_status_t epd_2in9b_display(epd_2in9b_t *dev, const uint8_t *bw_plane, const uint8_t *red_plane);
epd_2in9b_status_t epd_2in9b_sleep(epd_2in9b_t *dev);
epd_2in9b_status_t epd_2in9b_wait_idle(epd_2in9b_t *dev);
bool epd_2in9b_is_initialized(const epd_2in9b_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* EPD_2IN9B_H */
