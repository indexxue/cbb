/**
 * @file    it7259.h
 * @brief   IT7259 电容触摸：I2C 回调、Query/Point 读点（与 MCU 解耦）。
 */

#ifndef IT7259_H
#define IT7259_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IT7259_I2C_ADDR_DEFAULT (0x46U)

#define IT7259_BUF_QUERY (0x80U)
#define IT7259_BUF_POINT (0xE0U)

#define IT7259_QUERY_POINT (0x80U)
#define IT7259_QUERY_BUSY (0x01U)

typedef int (*it7259_i2c_write_read_t)(uint8_t addr7,
                                       const uint8_t *write_data,
                                       uint16_t write_len,
                                       uint8_t *read_data,
                                       uint16_t read_len);
typedef void (*it7259_delay_ms_t)(uint32_t ms);

typedef enum {
    IT7259_OK = 0,
    IT7259_ERROR_PARAM,
    IT7259_ERROR_I2C,
    IT7259_ERROR_NOT_INIT,
    IT7259_ERROR_BUSY,
    IT7259_ERROR_NO_POINT,
} it7259_status_t;

typedef struct {
    it7259_i2c_write_read_t write_read;
    it7259_delay_ms_t delay_ms;
    uint8_t address;     /* 7-bit，默认 0x46 */
    uint16_t panel_w;    /* 触摸原始量程宽，0 则按 240 */
    uint16_t panel_h;    /* 触摸原始量程高，0 则按 240 */
    uint16_t disp_w;     /* 映射到显示宽，0 则 240 */
    uint16_t disp_h;     /* 映射到显示高，0 则 240 */
    bool swap_xy;
    bool invert_x;
    bool invert_y;
} it7259_config_t;

typedef struct {
    it7259_i2c_write_read_t write_read;
    it7259_delay_ms_t delay_ms;
    uint8_t address;
    uint16_t panel_w;
    uint16_t panel_h;
    uint16_t disp_w;
    uint16_t disp_h;
    bool swap_xy;
    bool invert_x;
    bool invert_y;
    bool initialized;
} it7259_t;

typedef struct {
    int16_t x;
    int16_t y;
    bool pressed;
} it7259_point_t;

it7259_status_t it7259_init(it7259_t *dev, const it7259_config_t *cfg);

/** 轮询读一点：无新报点时返回 IT7259_ERROR_NO_POINT（可保持上次状态）。 */
it7259_status_t it7259_read_point(it7259_t *dev, it7259_point_t *out);

bool it7259_is_initialized(const it7259_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* IT7259_H */
