# FD6288Q 三相栅极驱动

本目录为 **Fortior FD6288Q**（QFN24，与 FD6288T 同系列）的通用 C 驱动，通过 **HIN/LIN 回调** 接入任意 MCU/HAL，不依赖 `Common/` 或具体工程。

## 器件说明

- 三相半桥栅极驱动，最高约 250 V 浮动通道；逻辑 3.3 V / 5 V 兼容。
- 输入 HIN1..3 / LIN1..3 与输出 HO/LO **同相**；内置直通防止与死区（典型 200 ns）。
- **无** I2C/SPI 寄存器，**无** 芯片 EN 脚；板级若有使能管 / Fault，用可选 GPIO 回调。

**本驱动不做**：FOC、BEMF 过零算法、FreeRTOS 任务、日志。

## 硬件参数（电气摘要）

| 项目 | 值 |
|------|-----|
| 接口 | HIN×3 + LIN×3（逻辑） |
| VCC | 5 V … 20 V |
| 内置死区 DT | 100 … 300 ns（typ 200） |
| 逻辑阈 | VIH≥2.7 V，VIL≤0.8 V |

## 设计说明

- 必填回调 `leg_apply(phase, mode, duty_permille)`：板级把 `LEG_PWM/LOW/FLOAT/HIGH` 映射到 TIM 互补通道或 GPIO。
- `fd6288q_set_step()` 使用标准六步表（正序）；相序不对时在板级交换 U/V/W 映射或改步表方向。
- 互补 PWM 仍建议在 MCU TIM 配置死区；芯片死区是第二道保护。

### 六步表（CW）

| step | U | V | W |
|------|---|---|---|
| 0 | PWM | LOW | FLOAT |
| 1 | PWM | FLOAT | LOW |
| 2 | FLOAT | PWM | LOW |
| 3 | LOW | PWM | FLOAT |
| 4 | LOW | FLOAT | PWM |
| 5 | FLOAT | LOW | PWM |

## 文件

| 文件 | 说明 |
|------|------|
| `fd6288q.h` | 类型、枚举、API |
| `fd6288q.c` | 实现 |

## API 概览

### 回调

```c
typedef void (*fd6288q_leg_apply_t)(uint8_t phase, fd6288q_leg_mode_t mode, uint16_t duty_permille);
typedef void (*fd6288q_gpio_set_t)(uint8_t pin_id, uint8_t level); /* ENABLE */
typedef int  (*fd6288q_gpio_get_t)(uint8_t pin_id);                /* FAULT, <0 = N/A */
```

### 常用 API

| 函数 | 作用 |
|------|------|
| `fd6288q_init` | 注册回调，三相 FLOAT，ENABLE 关 |
| `fd6288q_enable` | 板级 ENABLE |
| `fd6288q_set_step` / `set_duty` | 六步换相 / 改占空比 |
| `fd6288q_coast` / `brake_low` | 滑行 / 低压侧制动 |
| `fd6288q_emergency_stop` | 关 ENABLE + FLOAT |

## 移植提示（STM32G474 flight-ctrl）

| 电机 | 芯片 | TIM | HIN1/2/3 | LIN | 过零 |
|------|------|-----|----------|-----|------|
| M1 | FD6288Q #1 | TIM1 | CH1/CH2/CH3 | CHxN | ADC2×3 + COMP4 |
| M2 | FD6288Q #2 | TIM8 | **CH3/CH2/CH1** | CHxN | COMP1–3 |

死区 **0.5 µs**：TIM `BDTR`（DTG=75 @150 MHz）+ `fd6288q` 软件（换相 FLOAT + `delay_ns`）。服务封装见 `services/motor_bridge.*`。

## 不负责

传感器换相算法、电流环、过流 Break 路由（板级 COMP/TIM Break）。
