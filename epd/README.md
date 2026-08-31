# EPD 多尺寸墨水屏驱动（panel profile）

本目录为 **SSD16xx 系电子墨水屏** 的通用 C 驱动：共享 SPI/GPIO 总线 + **panel profile** 描述分辨率与刷屏协议。绘图层 `epd_display` 使用运行时宽高与**板级提供的帧缓冲**。

| Profile | 模组 | 分辨率 | 颜色 |
|---------|------|--------|------|
| `epd_panel_2in9_bw_otp` | 2.9" BW，SSD1680 OTP（与 GDEY 同 init） | 128×296 | BW |
| `epd_panel_gdey029z95` | GoodDisplay GDEY029Z95 | 128×296 | BWR |
| `epd_panel_gdey0213z98` | GoodDisplay GDEY0213Z98（OTP Y递减） | 122×250 | BWR |
| `epd_panel_gdey0213z98_gx` | GDEY0213Z98 / GxEPD2 时序 | 122×250 | BWR |
| `epd_panel_ws_2in13b_v4` | Waveshare 2.13" B V4 | 122×250 | BWR |
| `epd_panel_ws_2in9_v2` | Waveshare 2.9" V2（主机 LUT） | 128×296 | BW |
| `epd_panel_ws_1in54_v2` | Waveshare 1.54" V2 | 200×200 | BW |
| `epd_panel_ws_1in54b_v2` | Waveshare 1.54" B V2 | 200×200 | BWR |
| `epd_panel_ws_2in13_v3` | Waveshare 2.13" V3 | 122×250 | BW |

VET6 工程通过 [`Common/Inc/epd_panel_select.h`](../../Common/Inc/epd_panel_select.h) 的 `EPD_PANEL_ID` 宏切换测试屏。

## 文件

| 路径 | 说明 |
|------|------|
| `epd.h` / `epd.c` | 总线句柄、wait_idle、写 RAM 辅助、panel 调度 |
| `epd_panel.h` | `epd_panel_info_t` 与 profile 声明 |
| `panels/*.c` | 各模组 init/clear/display/sleep |
| `epd_display.*` | 帧缓冲绘图、字库、产测 |
| `epd_display_assets.*` / `epd_display_font.h` | 图标与 ASCII 点阵 |

## 板级接入要点

```c
#include "epd_display.h"
#include "epd_panel.h"

static uint8_t s_bw[4736];
static uint8_t s_red[4736];
static epd_display_t s_disp;

epd_display_config_t cfg = {
    .epd = {
        .spi_tx = ...,
        .set_cs = ...,
        .set_dc = ...,
        .set_rst = ...,
        .read_busy = ...,
        .delay_ms = ...,
        .busy_active_high = true,
        .bw_invert = true,           /* GDEY029Z95 通常需要 */
        .panel = &epd_panel_gdey029z95,
    },
    .bw = s_bw,
    .red = s_red,
    .plane_capacity = sizeof(s_bw),
    .hw_clear_on_init = false,
};
epd_display_init(&s_disp, &cfg);
```

## 编入工程

1. Include Path 追加 `../cbb/epd`
2. 编译：`epd.c`、`epd_display.c`、`epd_display_assets.c`、所需 `panels/epd_panel_*.c`
