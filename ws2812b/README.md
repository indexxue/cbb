# WS2812B RGB 灯带驱动

本目录为 **WS2812 / WS2812B** 可寻址 RGB LED 的通用 C 驱动：维护 **GRB 像素缓冲** 与刷新 API，通过可插拔后端把数据送到灯带，**不绑定** 单一 MCU 或 ESP-IDF。

## 灯带是做什么的？

WS2812B 用 **一根数据线** 级联多颗 LED：每颗 24 bit（**G→R→B**），时序由协议规定。MCU 按 GRB 顺序串行输出后，整串同时锁存显示。

| 应用 | 说明 |
|------|------|
| 状态指示 | 单灯或少量 LED 显示电量、告警颜色 |
| 灯带 / 面板 | 氛围灯、条形指示、简易像素屏 |
| 产测 | 确认 GPIO/SPI/RMT 时序与供电正常 |

本驱动 **不负责** 特效算法（彩虹、FFT 等），只提供缓冲、`set_pixel_*`、`refresh`。

## 硬件与协议要点

| 项目 | 说明 |
|------|------|
| 数据线电平 | 多数模块为 5 V 逻辑；3.3 V MCU 建议电平匹配或确认模块兼容 |
| 供电 | LED 电流走独立 5 V，勿仅靠 MCU IO 供电 |
| 像素顺序 | 缓冲区内为 **GRB**；`ws2812b_set_pixel_rgb` 按 RGB 语义写入并自动转换 |
| 复位时间 | 帧末需 >50 µs 低电平；各后端须满足（RMT/SPI 编码已含） |

## 文件

| 文件 | 说明 |
|------|------|
| `ws2812b.h` / `ws2812b.c` | 通用像素 API + **USER** / **SPI** 后端 |
| `ws2812b_esp32.h` / `ws2812b_esp32.c` | ESP-IDF **RMT** 后端（仅 ESP 工程编译） |
| `README.md` | 本文档 |

## 后端选择

| 后端 | 宏 / 初始化 | 适用场景 |
|------|-------------|----------|
| **USER** | `WS2812B_BUS_USER`，`ws2812b_init_user()` | **单总线 GPIO  bit-bang**、定时器+DMA、RP2040 PIO、STM32 SPI+DMA 自定义等 |
| **SPI** | `WS2812B_BUS_SPI`，`ws2812b_init_spi()` | SPI **单线 MOSI** 输出：每 WS2812 bit 编码为 1 个 SPI 字节（8 字节/GRB 字节） |
| **ESP32 RMT** | `ws2812b_esp32_init()` | ESP-IDF 硬件 RMT，时序与官方 led_strip 示例一致 |

判断规则：`config.spi_write != NULL` 且 `bus == WS2812B_BUS_SPI` 走 SPI；否则用 `transmit`（USER）。

## API 概览

### 状态码 `ws2812b_status_t`

| 值 | 含义 |
|----|------|
| `WS2812B_OK` | 成功 |
| `WS2812B_ERROR_PARAM` | 参数非法 |
| `WS2812B_ERROR_NOT_INIT` | 未初始化 |
| `WS2812B_ERROR_IO` | 回调传输失败 |
| `WS2812B_ERROR_NOMEM` | 内存不足 |

### 常用函数

| 函数 | 说明 |
|------|------|
| `ws2812b_init_with_config()` | 完整配置（USER 或 SPI） |
| `ws2812b_init_user()` | 仅注册 `transmit` 回调 |
| `ws2812b_init_spi()` | SPI 编码 + `spi_write` 回调 |
| `ws2812b_esp32_init()` | ESP32 RMT（见下） |
| `ws2812b_set_pixel_rgb()` / `set_pixel_grb()` | 写缓冲，不发光 |
| `ws2812b_refresh()` | 输出当前缓冲 |
| `ws2812b_clear()` | 全零并刷新 |
| `ws2812b_get_pixels()` | 直接操作 GRB 缓冲 |

### 回调

```c
/* USER：收到原始 GRB，长度 = 3 * num_leds，返回 0 成功 */
typedef int (*ws2812b_transmit_t)(const uint8_t *grb, size_t len, void *ctx);

/* SPI：收到已展开缓冲，长度 = 3 * num_leds * 8 */
typedef int (*ws2812b_spi_write_t)(const uint8_t *buf, size_t len, void *ctx);
```

## 使用示例 — ESP32（RMT）

```c
#include "ws2812b.h"
#include "ws2812b_esp32.h"

static ws2812b_t strip;

void app_led_init(void)
{
    ws2812b_esp32_config_t cfg = {
        .gpio_num  = GPIO_NUM_8,
        .num_leds  = 16,
    };
    ESP_ERROR_CHECK(ws2812b_esp32_init(&strip, &cfg));
}

void app_led_red(void)
{
    ws2812b_set_pixel_rgb(&strip, 0, 255, 0, 0);
    ws2812b_refresh(&strip);
}
```

工程需加入：`ws2812b.c`、`ws2812b_esp32.c`，并链接 ESP-IDF `driver`（RMT）。

## 使用示例 — 单总线 GPIO（USER / STM32）

在 **关中断或临界区** 内按 WS2812 时序 bit-bang（具体延时与主频相关，需示波器校准）：

```c
static ws2812b_t strip;

static void ws_bit(uint8_t one)
{
    if (one) {
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
        /* T1H ~0.7µs */
        for (volatile int i = 0; i < 40; i++) { }
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
        for (volatile int i = 0; i < 20; i++) { }
    } else {
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
        for (volatile int i = 0; i < 16; i++) { }
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
        for (volatile int i = 0; i < 36; i++) { }
    }
}

static void ws_byte(uint8_t b)
{
    for (int i = 7; i >= 0; i--) {
        ws_bit((b >> i) & 1u);
    }
}

static int strip_transmit(const uint8_t *grb, size_t len, void *ctx)
{
    (void)ctx;
    uint32_t prim = __get_PRIMASK();
    __disable_irq();
    for (size_t i = 0; i < len; i++) {
        ws_byte(grb[i]);
    }
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
    /* reset >50µs */
    for (volatile uint32_t d = 0; d < 800; d++) { }
    __set_PRIMASK(prim);
    return 0;
}

void app_strip_init(void)
{
    ws2812b_init_user(&strip, 8, strip_transmit, NULL);
}
```

也可用 TIM+DMA、LL 库等实现 `transmit`，只要满足时序即可。

## 使用示例 — SPI 单线（MOSI）

将 SPI 设为 **约 2.4–3.2 MHz**，仅使用 MOSI 接灯带 DIN。驱动把每个 GRB bit 编成 1 字节（默认 `0`→`0xC0`，`1`→`0xFC`，可用 `spi_codeword0/1` 覆盖）：

```c
static ws2812b_t strip;

static int strip_spi_write(const uint8_t *buf, size_t len, void *ctx)
{
    SPI_HandleTypeDef *h = (SPI_HandleTypeDef *)ctx;
    return (HAL_SPI_Transmit(h, (uint8_t *)buf, (uint16_t)len, 1000) == HAL_OK) ? 0 : -1;
}

void app_strip_spi_init(void)
{
    ws2812b_init_spi(&strip, 8, strip_spi_write, &hspi1);
    /* CubeMX: SPI Mode 0, 8-bit, prescaler 使 SCK ≈ 2.4–3.2 MHz */
}
```

若颜色错误或闪烁，用示波器微调 `spi_codeword0/1` 或 SPI 时钟。

## 编入工程

| 平台 | 源文件 | 说明 |
|------|--------|------|
| STM32 / 通用 | `ws2812b.c` | 实现 USER/SPI；板级实现回调 |
| ESP-IDF | `ws2812b.c` + `ws2812b_esp32.c` | RMT 后端 |

1. 加入用到的 `.c`，Include Path 增加 `cbb/ws2812b`。
2. **不要** 在 `cbb` 内引用 `Common` / `Core`。
3. ESP 工程不要链接未使用的 `ws2812b_esp32.c` 到非 ESP 目标。

## 从旧版 API 迁移

旧版头文件直接依赖 `esp_err.h` / `gpio_num_t`，`ws2812b_init()` 仅 RMT。

| 旧 | 新 |
|----|-----|
| `ws2812b_init(dev, &cfg)` + `gpio_num` | `ws2812b_esp32_init(dev, &esp_cfg)` |
| `esp_err_t` 返回值 | 像素 API 为 `ws2812b_status_t`；ESP 入口仍为 `esp_err_t` |
| — | STM32 bit-bang → `ws2812b_init_user` + `transmit` |
| — | STM32 SPI → `ws2812b_init_spi` |

## 参考

- [WS2812B 数据手册](https://cdn-shop.adafruit.com/datasheets/WS2812B.pdf)（时序）
- ESP-IDF `examples/peripherals/rmt/led_strip`（RMT 编码参数来源）
