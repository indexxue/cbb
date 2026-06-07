# EPD 2.9 寸电子墨水屏驱动

本目录为 **2.9 寸黑白电子墨水屏（128×296）** 的通用 C 驱动，通过 SPI/GPIO 回调接入任意 MCU/HAL，不依赖 `Common` 或具体工程。

常见模组为 Waveshare **2.9inch e-Paper Module**（SPI 接口）。市面存在两代驱动 IC，本库通过 `variant` 区分：

| `epd_2in9_variant_t` | 驱动 IC | 说明 |
|----------------------|---------|------|
| `EPD_2IN9_VARIANT_V1` | SSD1606 | 旧版全刷/局刷 LUT（30 字节） |
| `EPD_2IN9_VARIANT_V2` | SSD1680 | V2 模组（默认推荐），支持快速刷新与局部刷新 |

注意：本驱动 **不负责** 绘图、字体、UI 或图像格式转换；上层需提供 `EPD_2IN9_FRAME_SIZE`（4736）字节的 1bpp 帧缓冲。

## 硬件参数

| 项目 | 值 |
|------|-----|
| 分辨率 | 128 × 296 |
| 接口 | SPI + D/C、RST、BUSY |
| 帧缓冲 | 4736 字节（`EPD_2IN9_FRAME_SIZE`） |
| 像素约定 | MSB 先出；1 = 白，0 = 黑 |
| BUSY | 默认 **高电平 = 忙**（`busy_active_high = true`） |

## 文件

| 文件 | 说明 |
|------|------|
| `epd_2in9.h` | 类型、宏、API |
| `epd_2in9.c` | 初始化、刷屏、睡眠 |

## API 概览

### 回调类型

```c
typedef void (*epd_2in9_spi_tx_t)(const uint8_t *data, uint16_t len);
typedef void (*epd_2in9_pin_out_t)(int high);
typedef int (*epd_2in9_pin_in_t)(void);   /* 读 BUSY：0/1 */
typedef void (*epd_2in9_delay_ms_t)(uint32_t ms);
```

`set_cs` 可选：若 SPI 外设由硬件 NSS 管理，可置 `NULL`。

### 返回值

| 值 | 含义 |
|----|------|
| `EPD_2IN9_OK` | 成功 |
| `EPD_2IN9_ERROR_PARAM` | 参数或必要回调缺失 |
| `EPD_2IN9_ERROR_NOT_INIT` | 未初始化 |
| `EPD_2IN9_ERROR_VARIANT` | 当前 API 不支持该 variant |

### 常用流程

| 函数 | 说明 |
|------|------|
| `epd_2in9_register()` | 保存回调与 variant，不访问硬件 |
| `epd_2in9_init()` | 标准初始化 |
| `epd_2in9_init_fast()` | V2 快速全刷 LUT |
| `epd_2in9_clear()` | 刷白 |
| `epd_2in9_display()` | 全刷显示（需 `EPD_2IN9_FRAME_SIZE` 帧缓冲） |
| `epd_2in9_display_stream()` | 按行回调生成数据并全刷（小 RAM MCU 推荐） |
| `epd_2in9_display_base()` | V2 双 RAM 基图（便于后续局刷） |
| `epd_2in9_display_partial()` | V2 局部刷新 |
| `epd_2in9_sleep()` | 深度睡眠（需重新 init） |

## 使用示例（STM32 HAL）

本工程墨水屏接 **SPI2**（硬件 NSS），控制脚见 `doc/IO引脚说明.md`：

| 信号 | 引脚 |
|------|------|
| SCK / MOSI / MISO / NSS | PB10 / PB15 / PB14 / PB12 |
| D/C | PA8 |
| BUSY | PB11 |
| RST | PB13 |

```c
#include "epd_2in9.h"
#include "spi.h"

static epd_2in9_t s_epd;
static uint8_t s_frame[EPD_2IN9_FRAME_SIZE];

static void epd_spi_tx(const uint8_t *data, uint16_t len)
{
    HAL_SPI_Transmit(&hspi2, (uint8_t *)data, len, HAL_MAX_DELAY);
}

static void epd_dc(int high)  { HAL_GPIO_WritePin(ESD_DC_GPIO_Port, ESD_DC_Pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET); }
static void epd_rst(int high) { HAL_GPIO_WritePin(ESD_RST_GPIO_Port, ESD_RST_Pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET); }
static int  epd_busy(void)    { return HAL_GPIO_ReadPin(ESD_BUSY_GPIO_Port, ESD_BUSY_Pin) == GPIO_PIN_SET ? 1 : 0; }

void epd_board_init(void)
{
    epd_2in9_config_t cfg = {
        .spi_tx = epd_spi_tx,
        .set_cs = NULL,           /* SPI2 硬件 NSS */
        .set_dc = epd_dc,
        .set_rst = epd_rst,
        .read_busy = epd_busy,
        .delay_ms = HAL_Delay,
        .variant = EPD_2IN9_VARIANT_V2,
        .busy_active_high = true,
        .lut_mode = EPD_2IN9_LUT_FULL,
    };
    epd_2in9_register(&s_epd, &cfg);
    epd_2in9_init(&s_epd);
    epd_2in9_clear(&s_epd);
}

void epd_show(const uint8_t *mono_bitmap)
{
    epd_2in9_display(&s_epd, mono_bitmap);
}
```

局部刷新前建议先 `epd_2in9_display_base()` 写入基图，之后对变化区域调用 `epd_2in9_display_partial()`。

## 编入工程

1. Keil 新建 Group `cbb/epd_2in9`，加入 `epd_2in9.c`。
2. Include Path 追加 `../cbb/epd_2in9`。
3. 在 `Common/` 或 `Core/` 实现 SPI/GPIO 回调，勿在 `cbb` 内引用 `hi2c` / `main.h`。

## 参考

- [Waveshare 2.9inch e-Paper Module Manual](https://www.waveshare.com/wiki/2.9inch_e-Paper_Module_Manual)
- [Waveshare e-Paper 官方 C 库](https://github.com/waveshareteam/e-Paper)
