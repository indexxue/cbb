# QMI8658A 六轴 IMU 驱动

本目录为 **QST QMI8658A / QMI8658** 的通用 C 驱动，通过 I2C 或 SPI 回调接入任意 MCU/HAL，不依赖 `Common` 或具体工程。

## IMU 是做什么的？

QMI8658A 是一颗 **6 轴惯性测量单元（IMU）**，在同一芯片内集成：

| 传感器 | 测量量 | 典型用途 |
|--------|--------|----------|
| **三轴加速度计** | 线加速度（含重力分量） | 倾斜角、运动检测、计步、震动 |
| **三轴陀螺仪** | 角速度 | 转向速率、姿态积分、防抖 |

二者组合后可做 **姿态估计**（与磁力计融合可得航向）、游戏手柄、可穿戴、无人机增稳、产测校验等。本驱动只完成 **寄存器配置、WHO_AM_I 校验、原始数据与芯片温度读取**，不包含卡尔曼滤波、四元数或倾角/航向融合算法。

## 硬件参数

| 项目 | 值 |
|------|-----|
| WHO_AM_I（`0x00`） | `0x05` |
| I2C 7 位地址 | `0x6A`（SA0 高，`QMI8658A_DEFAULT_I2C_ADDR_SA0_HIGH`）或 `0x6B`（SA0 低） |
| SPI | Mode 0 / Mode 3，最高约 15 MHz；4 线默认，3 线需 `spi_3wire = true` |
| 加速度量程 | ±2 / ±4 / ±8 / ±16 g |
| 陀螺量程 | ±16 … ±2048 °/s（见 `qmi8658a_gyro_range_t`） |
| 默认 ODR | CTRL2/CTRL3 中 `0x05`（约 250 Hz 加速度列 / 6DOF 陀螺档） |
| 数据格式 | 16 位有符号，小端（寄存器低位在前） |

## 设计说明

- **回调注入**：I2C 用 `write` / `read` 或 `write_read`；SPI 用 `spi_xfer`（整段事务期间 **CS 由板级拉低**）。
- **总线选择**：`config.spi_xfer != NULL` 走 SPI，否则走 I2C（须提供 `write`，且 `write_read` 或 `read` 至少其一）。
- **7 位 I2C 地址**：回调参数 `addr` 为 7 位；STM32 HAL 需 `(addr << 1)` 作为 `DevAddress`。
- **SPI 首字节**：bit7 = 1 读 / 0 写，低 7 位为寄存器地址（可用 `QMI8658A_SPI_ADDR_READ/WRITE`）。
- **初始化流程**：关传感器 → 软复位 → 校验 WHO_AM_I → 开地址自增（CTRL1）→ 配置量程与 ODR → 使能加速度计+陀螺（CTRL7）。
- **默认量程**（`qmi8658a_init()` / `qmi8658a_init_spi()`）：±2 g、±2048 °/s。

## 文件

| 文件 | 说明 |
|------|------|
| `qmi8658a.h` | 类型、宏、API 声明 |
| `qmi8658a.c` | 寄存器读写与初始化 |
| `README.md` | 本文档 |

## API 概览

### 回调类型

```c
typedef int (*qmi8658a_i2c_write_t)(uint8_t addr, const uint8_t *data, uint16_t len);
typedef int (*qmi8658a_i2c_read_t)(uint8_t addr, uint8_t *data, uint16_t len);
typedef int (*qmi8658a_i2c_write_read_t)(uint8_t addr,
                                         const uint8_t *write_data, uint16_t write_len,
                                         uint8_t *read_data, uint16_t read_len);
typedef int (*qmi8658a_spi_xfer_t)(const uint8_t *tx, uint8_t *rx, uint16_t len);
typedef void (*qmi8658a_delay_ms_t)(uint32_t ms);
```

I2C：`write` 与 `write_read` 至少提供其一，读寄存器时优先 `write_read`。  
SPI：`spi_xfer` 为全双工，仅发时可 `rx = NULL`，仅收时 `tx` 填 0。

### 返回值 `qmi8658a_status_t`

| 值 | 含义 |
|----|------|
| `QMI8658A_OK` | 成功 |
| `QMI8658A_ERROR_I2C` | 总线失败（I2C 或 SPI） |
| `QMI8658A_ERROR_NOT_INIT` | 未初始化或回调缺失 |
| `QMI8658A_ERROR_PARAM` | 参数非法 |
| `QMI8658A_ERROR_ID` | WHO_AM_I 不是 `0x05` |

### 初始化

| 函数 | 说明 |
|------|------|
| `qmi8658a_init()` | I2C 快捷初始化（默认 ±2 g / ±2048 °/s） |
| `qmi8658a_init_spi()` | SPI 快捷初始化（4 线，同上默认量程） |
| `qmi8658a_init_with_config()` | 自定义 `qmi8658a_config_t`（总线、地址、量程、3 线 SPI 等） |

### 运行中与数据

| 函数 | 说明 |
|------|------|
| `qmi8658a_set_accel_range()` | 设置加速度满量程 |
| `qmi8658a_set_gyro_range()` | 设置陀螺满量程 |
| `qmi8658a_read_raw()` | 读 ax/ay/az/gx/gy/gz 原始 LSB（`int16_t`） |
| `qmi8658a_read_temperature()` | 读芯片温度（°C，`raw / 256`） |

原始值换算物理量需按当前量程与数据手册 LSB 灵敏度自行计算；本驱动不封装 `m/s²` 或 `°/s` 浮点接口。

## 使用示例（I2C + STM32 HAL）

```c
#include "qmi8658a.h"
#include "i2c.h"

static qmi8658a_t imu;

static uint16_t imu_hal_addr(uint8_t addr_7bit)
{
    return (uint16_t)((uint16_t)addr_7bit << 1);
}

static int imu_i2c_write(uint8_t addr, const uint8_t *data, uint16_t len)
{
    return (HAL_I2C_Master_Transmit(&hi2c1, imu_hal_addr(addr),
                                    (uint8_t *)data, len, 100) == HAL_OK) ? 0 : -1;
}

static int imu_i2c_write_read(uint8_t addr, const uint8_t *tx, uint16_t tx_len,
                              uint8_t *rx, uint16_t rx_len)
{
    if (tx_len != 1u) {
        return -1;
    }
    return (HAL_I2C_Mem_Read(&hi2c1, imu_hal_addr(addr), tx[0],
                             I2C_MEMADD_SIZE_8BIT, rx, rx_len, 100) == HAL_OK) ? 0 : -1;
}

void app_imu_init(void)
{
    qmi8658a_config_t cfg = {
        .write      = imu_i2c_write,
        .read       = NULL,
        .write_read = imu_i2c_write_read,
        .delay_ms   = HAL_Delay,
        .address    = QMI8658A_DEFAULT_I2C_ADDR_SA0_HIGH,
        .accel_range = QMI8658A_ACCEL_RANGE_2G,
        .gyro_range  = QMI8658A_GYRO_RANGE_2048DPS,
    };

    if (qmi8658a_init_with_config(&imu, &cfg) != QMI8658A_OK) {
        /* 处理错误 */
    }
}

void app_imu_poll(void)
{
    int16_t ax, ay, az, gx, gy, gz;

    if (qmi8658a_read_raw(&imu, &ax, &ay, &az, &gx, &gy, &gz) == QMI8658A_OK) {
        /* 使用原始 LSB */
    }
}
```

或使用快捷接口：

```c
qmi8658a_init(&imu, QMI8658A_DEFAULT_I2C_ADDR_SA0_HIGH, imu_i2c_write, NULL, HAL_Delay);
/* 须改用 write_read 时仍推荐 init_with_config */
```

## 使用示例（SPI + STM32 HAL）

板级在 `spi_xfer` 内完成 **CS 拉低 → 传输 → CS 拉高**：

```c
#include "qmi8658a.h"
#include "spi.h"
#include "gpio.h"

static qmi8658a_t imu;

static int imu_spi_xfer(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_RESET);
    if (HAL_SPI_TransmitReceive(&hspi1, (uint8_t *)tx, rx, len, 100) != HAL_OK) {
        HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);
        return -1;
    }
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);
    return 0;
}

void app_imu_spi_init(void)
{
    if (qmi8658a_init_spi(&imu, imu_spi_xfer, HAL_Delay) != QMI8658A_OK) {
        /* 处理错误 */
    }
}
```

3 线 SPI（SDIO 半双工）时：

```c
qmi8658a_config_t cfg = {
    .spi_xfer   = imu_spi_xfer,
    .delay_ms   = HAL_Delay,
    .spi_3wire  = true,
    .accel_range = QMI8658A_ACCEL_RANGE_2G,
    .gyro_range  = QMI8658A_GYRO_RANGE_2048DPS,
};
qmi8658a_init_with_config(&imu, &cfg);
```

## 编入工程

1. 将 `qmi8658a.c` 加入 Keil/IAR 等工程，头文件路径增加 `cbb/qmi8658a`。
2. 在板级实现 I2C 或 SPI 回调（见上例）。
3. **不要** 在 `cbb` 内引用 `Common`（见 `doc/STM32G431_development_standard.md`）。

## 参考

- [QMI8658A 数据手册](https://files.waveshare.com/upload/5/5f/QMI8658A_Datasheet_Rev_A.pdf)（寄存器、SPI 协议、量程与 ODR）
