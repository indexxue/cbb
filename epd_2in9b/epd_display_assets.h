/**
 * @file    epd_display_assets.h
 * @brief   内置图形与图标资源（MSB 先出，1=前景像素）。
 */

#ifndef EPD_DISPLAY_ASSETS_H
#define EPD_DISPLAY_ASSETS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EPD_GFX_ICON_HEART_16 = 0,
    EPD_GFX_ICON_CHECK_16,
    EPD_GFX_ICON_ARROW_16,
    EPD_GFX_BITMAP_CHECKER_32,
    EPD_GFX_ASSET_COUNT,
} epd_gfx_asset_id_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    const uint8_t *data;
} epd_gfx_asset_t;

const epd_gfx_asset_t *epd_gfx_asset_get(epd_gfx_asset_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* EPD_DISPLAY_ASSETS_H */
