/**
 * @file    ft6336.h
 * @brief   FT6336 电容触摸：I2C 回调、单点/双点读点（与 MCU 解耦）。
 */

#ifndef FT6336_H
#define FT6336_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FT6336_I2C_ADDR_DEFAULT (0x38U)
/** FT6336U；同族 FT6206=0x06、FT6236=0x36 亦接受 */
#define FT6336_CHIP_ID_EXPECT (0x64U)
#define FT6336_CHIP_ID_FT6206 (0x06U)
#define FT6336_CHIP_ID_FT6236 (0x36U)
#define FT6336_MAX_POINTS (2U)

typedef int (*ft6336_i2c_write_read_t)(uint8_t addr7,
                                      const uint8_t *write_data,
                                      uint16_t write_len,
                                      uint8_t *read_data,
                                      uint16_t read_len);
typedef void (*ft6336_pin_t)(int high);
typedef void (*ft6336_delay_ms_t)(uint32_t ms);

typedef enum {
    FT6336_OK = 0,
    FT6336_ERROR_PARAM,
    FT6336_ERROR_I2C,
    FT6336_ERROR_NOT_INIT,
    FT6336_ERROR_ID,
    FT6336_ERROR_NO_POINT,
} ft6336_status_t;

typedef struct {
    ft6336_i2c_write_read_t write_read; /* 必需 */
    ft6336_pin_t set_rst;               /* 可选；NULL = 无硬件复位 */
    ft6336_delay_ms_t delay_ms;
    uint8_t address;  /* 7-bit；0 → 0x38 */
    uint16_t panel_w; /* 触摸原始量程宽，0 → 320 */
    uint16_t panel_h; /* 触摸原始量程高，0 → 480 */
    uint16_t disp_w;  /* 映射到显示宽，0 → panel_w */
    uint16_t disp_h;  /* 映射到显示高，0 → panel_h */
    bool swap_xy;
    bool invert_x;
    bool invert_y;
} ft6336_config_t;

typedef struct {
    ft6336_i2c_write_read_t write_read;
    ft6336_pin_t set_rst;
    ft6336_delay_ms_t delay_ms;
    uint8_t address;
    uint16_t panel_w;
    uint16_t panel_h;
    uint16_t disp_w;
    uint16_t disp_h;
    bool swap_xy;
    bool invert_x;
    bool invert_y;
    bool initialized;
} ft6336_t;

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t id; /* 触点 ID（0/1） */
    bool pressed;
} ft6336_point_t;

ft6336_status_t ft6336_init(ft6336_t *dev, const ft6336_config_t *cfg);

/** 最近一次读到的 CHIP_ID（`0xA3`）；未读过为 0。 */
uint8_t ft6336_chip_id(const ft6336_t *dev);

/** 轮询读第一触点；无触点时返回 FT6336_ERROR_NO_POINT（pressed=false）。 */
ft6336_status_t ft6336_read_point(ft6336_t *dev, ft6336_point_t *out);

/**
 * 轮询读最多 `max_points` 个触点（硬上限 2）。
 * `out_count` 输出有效触点数；无触点时返回 FT6336_ERROR_NO_POINT 且 *out_count=0。
 */
ft6336_status_t ft6336_read_points(ft6336_t *dev,
                                   ft6336_point_t *out,
                                   uint8_t max_points,
                                   uint8_t *out_count);

bool ft6336_is_initialized(const ft6336_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* FT6336_H */
