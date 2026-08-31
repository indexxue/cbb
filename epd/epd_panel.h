/**
 * @file    epd_panel.h
 * @brief   墨水屏 panel profile：分辨率、色平面与 init/刷屏 ops。
 */

#ifndef EPD_PANEL_H
#define EPD_PANEL_H

#include "epd.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct epd_panel_info {
    const char *name;
    uint16_t width;
    uint16_t height;
    /** 1 = 纯黑白，2 = 黑白+红。 */
    uint8_t planes;
    /** Driver Output / data entry 参考值（写 0x11）；各 panel 自管 init。 */
    uint8_t data_entry;
    /** 全刷 Display Update Control2（写 0x22）。 */
    uint8_t update_full;
    /** true：绘图层逻辑 y 映射到 RAM 时做垂直翻转（GDEY029Z95）。 */
    bool flip_y;
    epd_status_t (*init)(epd_t *dev);
    epd_status_t (*clear)(epd_t *dev);
    /**
     * 刷屏。BW 屏可忽略 red（传 NULL）；BWR 需要 bw+red。
     * 单色快捷路径：red == NULL 且 planes==2 时由 panel 写空红层。
     */
    epd_status_t (*display)(epd_t *dev, const uint8_t *bw, const uint8_t *red);
    epd_status_t (*sleep)(epd_t *dev);
} epd_panel_info_t;

static inline uint16_t epd_panel_bytes_per_row(const epd_panel_info_t *panel)
{
    if (panel == NULL) {
        return 0U;
    }
    return (uint16_t)((panel->width + 7U) / 8U);
}

static inline uint32_t epd_panel_plane_size(const epd_panel_info_t *panel)
{
    if (panel == NULL) {
        return 0U;
    }
    return (uint32_t)epd_panel_bytes_per_row(panel) * (uint32_t)panel->height;
}

/** GoodDisplay S-GDEY029Z95，128×296 BWR，SSD1680。 */
extern const epd_panel_info_t epd_panel_gdey029z95;

/** GoodDisplay GDEY0213Z98，122×250 BWR（OTP Y递减，同 GDEY029 族）。 */
extern const epd_panel_info_t epd_panel_gdey0213z98;

/** GDEY0213Z98 / GxEPD2_213_Z98c 时序（data entry 0x03，Y 递增）。 */
extern const epd_panel_info_t epd_panel_gdey0213z98_gx;

/** Waveshare 2.13" e-Paper (B) V4，122×250 BWR，SSD1680。 */
extern const epd_panel_info_t epd_panel_ws_2in13b_v4;

/** 2.9" 纯黑白，SSD1680 OTP（与 GDEY029Z95 同 init，无红平面）。 */
extern const epd_panel_info_t epd_panel_2in9_bw_otp;

/** Waveshare 2.9" e-Paper Module V2，128×296 BW，SSD1680。 */
extern const epd_panel_info_t epd_panel_ws_2in9_v2;

/** Waveshare 1.54" e-Paper Module V2，200×200 BW，SSD1681。 */
extern const epd_panel_info_t epd_panel_ws_1in54_v2;

/** Waveshare 1.54" e-Paper (B) V2，200×200 BWR，SSD1680。 */
extern const epd_panel_info_t epd_panel_ws_1in54b_v2;

/** Waveshare 2.13" e-Paper V3，122×250 BW，SSD1680。 */
extern const epd_panel_info_t epd_panel_ws_2in13_v3;

#ifdef __cplusplus
}
#endif

#endif /* EPD_PANEL_H */
