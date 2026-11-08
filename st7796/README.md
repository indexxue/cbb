# ST7796 TFT 驱动

本目录为 **Sitronix ST7796** 的通用 C 驱动，通过 **SPI + GPIO** 回调接入任意 MCU/HAL，不依赖 `Common` 或具体工程。

## 器件说明

ST7796 常见于 3.5" 320×480 TFT 模组，支持 RGB565 显存流式写入。

**本驱动不做**：UI 绘制、字体、FreeRTOS 任务、DMA 调度。板级负责 SPI 吞吐与背光电源。

## 硬件参数（本驱动默认）

| 项目 | 值 |
|------|-----|
| 接口 | SPI（板级实现 `spi_tx`） |
| 分辨率 | 320×480（竖屏）；横屏逻辑 480×320 |
| 像素格式 | RGB565，总线大端 |
| 片选 | 软件 CS（`set_cs`）；ESP-IDF 请设 `spics_io_num = -1` |
| 复位 / 背光 | 可选 `set_rst` / `set_bl` |

## 设计说明

- 回调注入：`spi_tx` + `set_cs` / `set_dc` / `set_rst` / `set_bl` + `delay_ms`。
- 初始化：硬件复位 → `SWRESET`/`SLPOUT` → 解锁命令集 → `MADCTL`/`COLMOD` → 电源/伽马 → `DISPON`。
- 写显存：`set_window` 发 `CASET`/`RASET`/`RAMWR` 后 **保持 CS 低**；多次 `write_pixels` / `write_pixel_bytes`；最后 `end_write` 释放 CS。
- 全幅窗口无 GRAM 偏移（与部分 ST7789 小屏不同）。
- `bgr` 非 0 时置 `MADCTL` bit3（多数模组默认 BGR）。

## 文件

| 文件 | 说明 |
|------|------|
| `st7796.h` | 类型、枚举、API |
| `st7796.c` | 初始化与写显存 |
| `st7796_config.h` | `ST7796_TX_CHUNK` 等可覆盖参数 |

## API 概览

### 回调类型

```c
typedef void (*st7796_spi_tx_t)(const uint8_t *data, uint16_t len);
typedef void (*st7796_pin_t)(int high);      /* 1=高，0=低；NULL 跳过 */
typedef void (*st7796_delay_ms_t)(uint32_t ms);
```

### 返回值 `st7796_status_t`

| 值 | 含义 |
|----|------|
| `ST7796_OK` | 成功 |
| `ST7796_ERROR_PARAM` | 参数非法 |
| `ST7796_ERROR_NOT_INIT` | 未初始化 |

### 主要 API

| 函数 | 说明 |
|------|------|
| `st7796_register` | 绑定回调并完成初始化 |
| `st7796_set_window` | 设置窗口并进入 `RAMWR`（CS 保持低） |
| `st7796_write_pixels` | 写 `uint16_t` RGB565（主机序 → 总线大端） |
| `st7796_write_pixel_bytes` | 写已排好的大端字节流 |
| `st7796_end_write` | 释放 CS |
| `st7796_set_backlight` | 背光开关 |
| `st7796_display_width/height` | 随 rotation 返回逻辑宽高 |

## 使用示例（STM32 HAL）

```c
#include "st7796.h"

extern SPI_HandleTypeDef hspi1;

static void lcd_spi_tx(const uint8_t *data, uint16_t len)
{
    (void)HAL_SPI_Transmit(&hspi1, (uint8_t *)data, len, 100);
}

static void lcd_cs(int high)  { HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET); }
static void lcd_dc(int high)  { HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET); }
static void lcd_rst(int high) { HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET); }
static void lcd_bl(int high)  { HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET); }

static st7796_t g_lcd;

void lcd_board_init(void)
{
    st7796_config_t cfg = {
        .spi_tx = lcd_spi_tx,
        .set_cs = lcd_cs,
        .set_dc = lcd_dc,
        .set_rst = lcd_rst,
        .set_bl = lcd_bl,
        .delay_ms = HAL_Delay,
        .rotation = ST7796_ROT_PORTRAIT_0,
        .bgr = 1,
    };
    (void)st7796_register(&g_lcd, &cfg);
}
```

填充示例：

```c
uint16_t line[320];
for (int i = 0; i < 320; i++) {
    line[i] = 0xF800U; /* red */
}
(void)st7796_set_window(&g_lcd, 0, 0, 319, 0);
(void)st7796_write_pixels(&g_lcd, line, 320);
st7796_end_write(&g_lcd);
```

## 编入工程

1. 加入 `st7796.c`，Include Path 追加 `cbb/st7796`
2. 板级实现回调；勿在本目录引用 `Common/` / HAL 句柄头
3. ESP-IDF：本库已在 `cbb/CMakeLists.txt` 注册

## 注意事项

- 红蓝颠倒：将 `cfg.bgr` 改为 `0`。
- 部分模组电源/伽马与厂商略有差异；若花屏可对照模组厂 init 表微调 `st7796.c` 中扩展命令。
- 大块刷新建议板级用 DMA `spi_tx`，并视 SRAM 增大 `ST7796_TX_CHUNK`。
