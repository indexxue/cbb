# HC-SR04 超声波测距模块

本目录为 **HC-SR04 / HC-SR04P** 的通用 C 驱动，通过 GPIO 与微秒计时回调接入任意 MCU/HAL，不依赖 `Common` 或具体工程。

## 模块是做什么的？

HC-SR04 通过 **Trig** 引脚发送 ≥10 µs 高电平触发脉冲，在 **Echo** 引脚上返回与声程成正比的回波高电平脉宽，可换算为距离。

| 应用 | 说明 |
|------|------|
| **避障** | 检测前方障碍物距离 |
| **跟随 / 停车** | 维持与障碍物的目标间距 |
| **产测** | 确认 Trig/Echo 接线与基本量程 |

注意：本驱动只负责 **触发、测脉宽、换算距离**，不做多传感器融合、滤波或 SLAM。附近软质/斜面、电机噪声会影响读数稳定性。

## 硬件参数（本工程默认）

| 项目 | 值 |
|------|-----|
| 接口 | Trig（GPIO 输出）+ Echo（GPIO 输入） |
| 供电 | **推荐 HC-SR04P（3.3 V）**；5 V 版 Echo 需电平转换 |
| 量程 | 约 2 cm ~ 400 cm（以数据手册为准） |
| 触发脉宽 | 默认 10 µs（`HC_SR04_TRIG_PULSE_US_DEFAULT`） |

> **TM4C123 car-4wd**：Echo 接 **PC1（SWDIO）**，须在调试烧录完成后再调用板级 `Board_Ultra_Init()`，否则 J-Link 可能无法再次连接。

## 设计说明

- **回调注入**：`gpio_set` / `gpio_read` / `delay_us` / `get_tick_us` 由板级实现。
- **引脚映射**：驱动内使用逻辑引脚 `HC_SR04_PIN_TRIG` / `HC_SR04_PIN_ECHO`，板级回调映射到实际 GPIO。
- **距离换算**：`distance_mm = pulse_us × speed_cm_per_s / 20000`（往返声程除以 2）。
- **默认声速**：34300 cm/s（约 20 °C），可通过 `hc_sr04_config_t.speed_cm_per_s` 覆盖。

## 文件

| 文件 | 说明 |
|------|------|
| `hc_sr04.h` | 类型、宏、API 声明 |
| `hc_sr04.c` | 触发时序、回波脉宽测量、距离换算 |

## API 概览

### 回调类型

```c
typedef void (*hc_sr04_gpio_set_t)(uint8_t pin_id, uint8_t level);
typedef uint8_t (*hc_sr04_gpio_read_t)(uint8_t pin_id);
typedef void (*hc_sr04_delay_us_t)(uint32_t us);
typedef uint32_t (*hc_sr04_get_tick_us_t)(void);
```

`get_tick_us` 须为单调递增的微秒计数器（DWT、`micros()` 等），用于测量 Echo 高电平脉宽。

### 返回值 `hc_sr04_status_t`

| 值 | 含义 |
|----|------|
| `HC_SR04_OK` | 成功 |
| `HC_SR04_ERROR_PARAM` | 参数非法 |
| `HC_SR04_ERROR_NOT_INIT` | 未初始化或回调缺失 |
| `HC_SR04_ERROR_TIMEOUT` | 等待 Echo 边沿超时 |
| `HC_SR04_ERROR_OUT_OF_RANGE` | 换算距离超出配置量程 |

### 主要函数

- `hc_sr04_init_with_config()` — 带默认量程/声速/超时配置初始化
- `hc_sr04_measure_mm()` — 测距，输出毫米
- `hc_sr04_measure_cm()` — 测距，输出厘米（整数截断）
- `hc_sr04_pulse_us_to_mm()` — 脉宽 → 距离换算（可单独用于测试）

## 板级移植示例（TM4C123）

```c
#include "hc_sr04.h"
#include "board.h"
#include "bsp_gpio.h"
#include "bsp_systick.h"
#include "bsp_sysctl.h"

static const bsp_gpio_pin_t s_trig = { GPIO_ULTRA_TRIG_PORT, GPIO_ULTRA_TRIG_MASK };
static const bsp_gpio_pin_t s_echo = { GPIO_ULTRA_ECHO_PORT, GPIO_ULTRA_ECHO_MASK };

static void ultra_gpio_set(uint8_t pin_id, uint8_t level)
{
    if (pin_id == HC_SR04_PIN_TRIG) {
        bsp_gpio_write(&s_trig, level != 0u);
    }
}

static uint8_t ultra_gpio_read(uint8_t pin_id)
{
    if (pin_id == HC_SR04_PIN_ECHO) {
        return bsp_gpio_read(&s_echo) ? 1u : 0u;
    }
    return 0u;
}

static uint32_t ultra_get_tick_us(void)
{
    static uint32_t cycles_per_us;
    if (cycles_per_us == 0u) {
        cycles_per_us = bsp_clock_get_hz() / 1000000u;
    }
    return DWT_CYCCNT / cycles_per_us;  /* 或使用工程内封装 */
}

void board_ultra_example(void)
{
    hc_sr04_t dev;
    uint16_t mm = 0u;

    (void)Board_Ultra_Init();
    (void)hc_sr04_init(&dev, ultra_gpio_set, ultra_gpio_read, bsp_delay_us, ultra_get_tick_us);
    if (hc_sr04_measure_mm(&dev, &mm) == HC_SR04_OK) {
        /* use mm */
    }
}
```

姊妹 STM32 工程：`gpio_set` → `HAL_GPIO_WritePin`；`gpio_read` → `HAL_GPIO_ReadPin`；`get_tick_us` → `HAL_GetTick() * 1000` 或 DWT。

## 依赖

- `<stdint.h>` / `<stdbool.h>` 仅标准 C 头文件，无 HAL 依赖。
