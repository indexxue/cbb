# EPD 2.9 寸红白黑墨水屏驱动

本目录为 **2.9 寸红白黑（BWR）电子墨水屏（128×296）** 的通用 C 驱动，通过 SPI/GPIO 回调接入任意 MCU/HAL，不依赖 `Common` 或具体工程。

协议参考 GoodDisplay **S-GDEY029Z95** 官方示例（`Display_EPD_W21`），驱动 IC 为 **SSD1680**（SSD1680A 系列）。

与同级目录 `epd_2in9`（Waveshare 纯黑白 SSD1680）的区别：

| 项目 | `epd_2in9` | `epd_2in9b`（本驱动） |
|------|------------|----------------------|
| 目标模组 | Waveshare 2.9" BW V2 | GoodDisplay GDEY029Z95 BWR |
| 颜色 | 黑 / 白 | 黑 / 白 / 红 |
| 初始化 `0x11` | `0x03`（Y 递增） | `0x01`（Y 递减） |
| LUT | 主机加载 Waveshare LUT | 模组内置 LUT |
| 全刷更新字节 | `0xC7` | `0xF7` |
| 帧缓冲 | 1 × 4736 B | 2 × 4736 B（BW + Red） |

注意：底层驱动不负责板级 HAL；绘图见 `epd_display`。板级绑定示例见主工程 `Common/Src/epd_display_board.c`。

## 硬件参数

| 项目 | 值 |
|------|-----|
| 分辨率 | 128 × 296 |
| 接口 | SPI + D/C、RST、BUSY（+ 可选 PWR） |
| 帧缓冲 | 2 × 4736 字节（`EPD_2IN9B_PLANE_SIZE`） |
| 黑白平面 | MSB 先出；1 = 白，0 = 黑 |
| 红色平面 | MSB 先出；1 = 红，0 = 非红 |
| BUSY | 默认 **高电平 = 忙**（`busy_active_high = true`） |
| 全刷耗时 | 约 4~6 s/帧（视 LUT 与环境温度） |

## 文件

| 文件 | 说明 |
|------|------|
| `epd_2in9b.h` / `epd_2in9b.c` | 寄存器协议、刷屏、睡眠 |
| `epd_display.h` / `epd_display.c` | 帧缓冲、绘图、字库、产测 |
| `epd_display_assets.h` / `epd_display_assets.c` | 内置图标/位图示例资源 |
| `epd_display_font.h` | ASCII 12/16/24 点阵字库 |

## API 概览

### 回调类型

```c
typedef void (*epd_2in9b_spi_tx_t)(const uint8_t *data, uint16_t len);
typedef void (*epd_2in9b_pin_out_t)(int high);
typedef int (*epd_2in9b_pin_in_t)(void);   /* 读 BUSY：0/1 */
typedef void (*epd_2in9b_delay_ms_t)(uint32_t ms);
```

`set_cs` 可选：若 SPI 外设由硬件 NSS 管理，可置 `NULL`。

### 配置项

| 字段 | 说明 |
|------|------|
| `busy_active_high` | `true`：BUSY 高 = 忙（GoodDisplay / SSD1680 默认） |
| `bw_invert` | `true`：写 RAM `0x24` 时对黑白平面按位取反；GDEY029Z95 通常需开启 |

### 返回值

| 值 | 含义 |
|----|------|
| `EPD_2IN9B_OK` | 成功 |
| `EPD_2IN9B_ERROR_PARAM` | 参数或必要回调缺失 |
| `EPD_2IN9B_ERROR_NOT_INIT` | 未初始化 |
| `EPD_2IN9B_ERROR_BUSY` | 等待 BUSY 超时（默认 45 s） |

### 常用流程

| 函数 | 说明 |
|------|------|
| `epd_2in9b_register()` | 保存回调，不访问硬件 |
| `epd_2in9b_init()` | 硬件 + 软件初始化（GoodDisplay `EPD_HW_Init`） |
| `epd_2in9b_clear()` | 刷白（写 `0x24` + `0x26`，全刷） |
| `epd_2in9b_display_mono()` | 仅写 `0x24` 并全刷；红色层保持上次内容 |
| `epd_2in9b_display()` | 写 `0x24` + `0x26` 并全刷（三色显示） |
| `epd_2in9b_sleep()` | 深度睡眠（需重新 init） |
| `epd_2in9b_wait_idle()` | 阻塞等待 BUSY 释放 |

### 显示接口对照（GoodDisplay 官方）

| 本驱动 API | GoodDisplay 官方函数 | 写 RAM | 更新字节 |
|------------|---------------------|--------|----------|
| `epd_2in9b_display_mono()` | `EPD_WhiteScreen_ALL()` | `0x24` | `0xF7` |
| `epd_2in9b_display()` | `EPD_SetRAMValue_BaseMap()` | `0x24` + `0x26` | `0xF7` |
| `epd_2in9b_clear()` | 双 RAM 填白 + 全刷 | `0x24` + `0x26` | `0xF7` |

## 使用示例（STM32 HAL）

本工程墨水屏接 **SPI3**（软件 CS），控制脚见 `doc/io_pin_assignment.md`：

| 信号 | 引脚 |
|------|------|
| SCK / MOSI / MISO | PC10 / PC12 / PC11 |
| CS / D/C / BUSY / RST | PD10 / PD13 / PD14 / PD15 |
| PWR（可选） | PE0 |

```c
#include "epd_2in9b.h"
#include "spi.h"
#include "main.h"

static epd_2in9b_t s_epd;
static uint8_t s_bw[EPD_2IN9B_PLANE_SIZE];
static uint8_t s_red[EPD_2IN9B_PLANE_SIZE];

static void epd_spi_tx(const uint8_t *data, uint16_t len)
{
    HAL_SPI_Transmit(&hspi3, (uint8_t *)data, len, HAL_MAX_DELAY);
}

static void epd_cs(int high)   { HAL_GPIO_WritePin(EPD_CS_GPIO_Port, EPD_CS_Pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET); }
static void epd_dc(int high)   { HAL_GPIO_WritePin(EPD_DC_GPIO_Port, EPD_DC_Pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET); }
static void epd_rst(int high)  { HAL_GPIO_WritePin(EPD_RST_GPIO_Port, EPD_RST_Pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET); }
static int  epd_busy(void)       { return HAL_GPIO_ReadPin(EPD_BUSY_GPIO_Port, EPD_BUSY_Pin) == GPIO_PIN_SET ? 1 : 0; }

void epd_board_init(void)
{
    epd_2in9b_config_t cfg = {
        .spi_tx = epd_spi_tx,
        .set_cs = epd_cs,
        .set_dc = epd_dc,
        .set_rst = epd_rst,
        .read_busy = epd_busy,
        .delay_ms = HAL_Delay,
        .busy_active_high = true,
        .bw_invert = true,       /* GDEY029Z95 通常需要 */
    };
    epd_2in9b_register(&s_epd, &cfg);
    epd_2in9b_init(&s_epd);
    epd_2in9b_clear(&s_epd);
}

/* 纯黑白显示 */
void epd_show_mono(const uint8_t *bw_bitmap)
{
    epd_2in9b_display_mono(&s_epd, bw_bitmap);
}

/* 红白黑显示：red_bitmap 中 1=红 */
void epd_show_bwr(const uint8_t *bw_bitmap, const uint8_t *red_bitmap)
{
    epd_2in9b_display(&s_epd, bw_bitmap, red_bitmap);
}
```

板级绑定与产测见主工程 `Common/Src/epd_display_board.c`（`epd_display_test_run(epd_display_board())` 等）。

### 内置图形/图标示例

驱动提供可直接调用的资源 ID 与演示接口：

```c
/* 绘制内置 16×16 心形图标 */
epd_display_draw_gfx_asset(disp, 4, 20, EPD_GFX_ICON_HEART_16, EPD_COLOR_BLACK, EPD_COLOR_WHITE);

/* 绘制基本图元样例（实心块、线框、斜线） */
epd_display_draw_shapes_sample(disp, 4, 24, EPD_COLOR_BLACK);

/* 整屏演示画面（图元 + 图标 + 棋盘格 + 红色心形） */
epd_display_gfx_demo_fill(disp);
epd_display_refresh(disp);

/* 或一步完成清屏、演示、三色刷新 */
epd_display_verify_gfx(disp);
```

内置资源：`EPD_GFX_ICON_HEART_16`、`EPD_GFX_ICON_CHECK_16`、`EPD_GFX_ICON_ARROW_16`、`EPD_GFX_BITMAP_CHECKER_32`。

## 编入工程

1. Keil 新建 Group `cbb/epd_2in9b`，加入 `epd_2in9b.c`。
2. Include Path 追加 `../cbb/epd_2in9b`。
3. 在 `Common/` 或 `Core/` 实现 SPI/GPIO 回调，勿在 `cbb` 内引用 `hspi3` / `main.h`。
4. 全刷阻塞约 4~6 s，请在 FreeRTOS 任务中调用并保证任务栈足够（帧缓冲建议放静态区）。
5. `epd_display_init()` 默认**不**做硬件清屏（`hw_clear_on_init = false`），上电后只刷一次即可较快出图；需要物理白屏时调用 `epd_display_clear_screen()`。

## 常见问题

**Q：刷全黑变成全白？**  
A：尝试切换 `bw_invert`（GDEY029Z95 与 Waveshare 源图极性可能相反）。

**Q：`EPD_2IN9B_ERROR_BUSY` 超时？**  
A：确认 BUSY 极性（`busy_active_high`）、SPI 接线、以及是否误用了 `epd_2in9` 的 Waveshare 初始化流程。

**Q：红色不显示？**  
A：三色必须用 `epd_2in9b_display(bw, red)`，不能只调 `display_mono()`；红色区域 BW 平面应为白（1）。

## 参考

- [GoodDisplay GDEY029Z95 资料](https://www.good-display.com/product/386.html)
- [GoodDisplay GDEY029Z95 STM32 例程](https://www.good-display.com/companyfile/1390.html)
- 同级 `epd_2in9`：Waveshare 2.9" 纯黑白 SSD1680
