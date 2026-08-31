/**
 * @file    epd.h
 * @brief   SSD16xx 墨水屏共享总线句柄（SPI/GPIO 回调，MCU 无关）。
 *
 * 尺寸与 init/刷屏差异由 panel profile 提供；本模块只提供命令总线与公共 RAM 写辅助。
 */

#ifndef EPD_H
#define EPD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*epd_spi_tx_t)(const uint8_t *data, uint16_t len);
typedef void (*epd_pin_out_t)(int high);
typedef int (*epd_pin_in_t)(void);
typedef void (*epd_delay_ms_t)(uint32_t ms);

typedef enum {
    EPD_OK = 0,
    EPD_ERROR_PARAM,
    EPD_ERROR_NOT_INIT,
    EPD_ERROR_BUSY,
    EPD_ERROR_UNSUPPORTED,
} epd_status_t;

struct epd_panel_info;

typedef struct {
    epd_spi_tx_t spi_tx;
    epd_pin_out_t set_cs;
    epd_pin_out_t set_dc;
    epd_pin_out_t set_rst;
    epd_pin_in_t read_busy;
    epd_delay_ms_t delay_ms;
    /** true：BUSY 高电平表示忙。 */
    bool busy_active_high;
    /**
     * true：写 RAM 0x24 时对黑白平面按位取反。
     * GDEY029Z95 等模组通常需开启。
     */
    bool bw_invert;
    const struct epd_panel_info *panel;
} epd_config_t;

typedef struct {
    epd_spi_tx_t spi_tx;
    epd_pin_out_t set_cs;
    epd_pin_out_t set_dc;
    epd_pin_out_t set_rst;
    epd_pin_in_t read_busy;
    epd_delay_ms_t delay_ms;
    bool busy_active_high;
    bool bw_invert;
    bool initialized;
    const struct epd_panel_info *panel;
} epd_t;

epd_status_t epd_register(epd_t *dev, const epd_config_t *cfg);
epd_status_t epd_init(epd_t *dev);
epd_status_t epd_clear(epd_t *dev);
epd_status_t epd_display(epd_t *dev, const uint8_t *bw, const uint8_t *red);
epd_status_t epd_sleep(epd_t *dev);
epd_status_t epd_wait_idle(epd_t *dev);
bool epd_is_initialized(const epd_t *dev);

uint16_t epd_width(const epd_t *dev);
uint16_t epd_height(const epd_t *dev);
uint16_t epd_bytes_per_row(const epd_t *dev);
uint32_t epd_plane_size(const epd_t *dev);

/* ---- 供 panel 实现使用的总线辅助（不对外业务层） ---- */

void epd_bus_delay_ms(epd_t *dev, uint32_t ms);
void epd_bus_hw_reset(epd_t *dev, uint32_t hold_ms, uint32_t settle_ms);
void epd_bus_write_cmd(epd_t *dev, uint8_t cmd);
void epd_bus_write_data(epd_t *dev, uint8_t data);
void epd_bus_write_data_buf(epd_t *dev, const uint8_t *buf, uint16_t len);
epd_status_t epd_bus_turn_on_display(epd_t *dev, uint8_t ctrl);
void epd_bus_write_plane_fill(epd_t *dev, uint8_t cmd, uint8_t fill, uint32_t plane_size);
void epd_bus_write_plane(epd_t *dev, uint8_t cmd, const uint8_t *plane, uint32_t plane_size,
                         bool invert);
epd_status_t epd_bus_check(const epd_t *dev, bool require_init);

#ifdef __cplusplus
}
#endif

#endif /* EPD_H */
