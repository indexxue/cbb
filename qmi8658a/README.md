# QMI8658A 六轴 IMU 驱动

本目录为 **QST QMI8658A / QMI8658** 的通用 C 驱动，通过 **I2C 或 SPI** 回调接入任意 MCU/HAL，不依赖 `Common` 或具体工程。

## 器件说明

QMI8658A 集成三轴加速度计与三轴陀螺仪，输出原始 16 位数据，可换算为 m/s²、dps 等物理量。

典型用途：姿态估计输入、运动检测、产测验证 IMU 通信与读数。

**本驱动不做**：传感器融合、Madgwick/Mahony 滤波、校准算法、FreeRTOS 任务。

## 硬件参数（本工程默认）

| 项目 | 值 |
|------|-----|
| 接口 | I2C 或 SPI（3 线 / 4 线，由 CTRL1.SIM 配置） |
| I2C 7 位地址 | `0x6A`（SA0 高 / 悬空）或 `0x6B`（SA0 低） |
| SPI 模式 | Mode 0 或 Mode 3（CPOL/CPHA 由硬件自动识别） |
| SPI 时钟 | 最高 15 MHz |
| WHO_AM_I（`0x00`） | `0x05` |
| 接口选择 | **CS=低**：SPI；**CS=高**：I2C/I3C |

## 设计说明

- **回调注入**：I2C 使用 `write` / `read` / `write_read`；SPI 使用 `spi_tx` + `spi_tx_rx` + `set_cs`。
- **SPI 地址字节**（数据手册 §16.2）：bit7=**1** 读、bit7=**0** 写；宏 `QMI8658A_SPI_ADDR_READ(reg)` / `QMI8658A_SPI_ADDR_WRITE(reg)` 已封装。
- **SPI 读时序**：一次 CS 事务内发送 `[reg\|0x80, 0x00…]`，全双工收 `[dummy, data…]`；驱动从 `rx[1]` 起取数据（与 SensorLib 一致）。
- **初始化**：软复位 → 读 WHO_AM_I → 写 CTRL1（ADDR_AI=1，可选 3 线 SPI）→ 配置量程并开启 accel+gyro。
- **默认配置**（`qmi8658a_init()` / `qmi8658a_init_spi()`）：±2 g、±2048 dps、ODR 约 250 Hz。

## 文件

| 文件 | 说明 |
|------|------|
| `qmi8658a.h` | 类型、枚举、I2C/SPI API |
| `qmi8658a.c` | 寄存器协议与初始化 |

## API 概览

### 回调类型

```c
/* I2C */
typedef int (*qmi8658a_i2c_write_t)(uint8_t addr, const uint8_t *data, uint16_t len);
typedef int (*qmi8658a_i2c_read_t)(uint8_t addr, uint8_t *data, uint16_t len);
typedef int (*qmi8658a_i2c_write_read_t)(uint8_t addr, ...);

/* SPI */
typedef int (*qmi8658a_spi_tx_t)(const uint8_t *tx, uint16_t len);
typedef int (*qmi8658a_spi_tx_rx_t)(const uint8_t *tx, uint8_t *rx, uint16_t len);
typedef void (*qmi8658a_set_cs_t)(uint8_t level);  /* 0=选中，1=释放 */
typedef void (*qmi8658a_delay_ms_t)(uint32_t ms);
```

SPI 初始化要求 `spi_tx`、`spi_tx_rx`、`set_cs` 均非 NULL。

### 返回值 `qmi8658a_status_t`

| 值 | 含义 |
|----|------|
| `QMI8658A_OK` | 成功 |
| `QMI8658A_ERROR_I2C` | I2C 总线失败 |
| `QMI8658A_ERROR_SPI` | SPI 总线失败 |
| `QMI8658A_ERROR_NOT_INIT` | 未初始化或回调缺失 |
| `QMI8658A_ERROR_PARAM` | 参数非法 |
| `QMI8658A_ERROR_ID` | WHO_AM_I 不是 `0x05` |

### 初始化

| 函数 | 说明 |
|------|------|
| `qmi8658a_init()` | I2C，默认地址与量程 |
| `qmi8658a_init_with_config()` | I2C，自定义 `qmi8658a_config_t` |
| `qmi8658a_init_spi()` | SPI，默认量程 |
| `qmi8658a_init_spi_with_config()` | SPI，自定义 `qmi8658a_spi_config_t`（含 `spi_3wire`） |

### 数据读取

| 函数 | 说明 |
|------|------|
| `qmi8658a_read_raw()` | 读 accel + gyro 原始 LSB |
| `qmi8658a_read_temperature()` | 读芯片温度（°C） |
| `qmi8658a_set_accel_range()` / `qmi8658a_set_gyro_range()` | 运行中改量程 |

### 换算关系（默认量程）

| 量程 | LSB 灵敏度 |
|------|------------|
| ±2 g | 16384 LSB/g |
| ±4 g | 8192 LSB/g |
| ±8 g | 4096 LSB/g |
| ±16 g | 2048 LSB/g |
| ±2048 dps | 16 LSB/dps |

## 使用示例（STM32 HAL）

### I2C

```c
#include "qmi8658a.h"

static int imu_i2c_write(uint8_t addr7, const uint8_t *data, uint16_t len)
{
    return HAL_I2C_Master_Transmit(&hi2c2, (uint16_t)(addr7 << 1), (uint8_t *)data, len, 100) == HAL_OK ? 0 : -1;
}

static int imu_i2c_wr_rd(uint8_t addr7, const uint8_t *w, uint16_t wlen, uint8_t *r, uint16_t rlen)
{
    return HAL_I2C_Mem_Read(&hi2c2, (uint16_t)(addr7 << 1), w[0], I2C_MEMADD_SIZE_8BIT, r, rlen, 100) == HAL_OK ? 0 : -1;
}

static qmi8658a_t g_imu;

void imu_init(void)
{
    qmi8658a_config_t cfg = {
        .write      = imu_i2c_write,
        .read       = NULL,
        .write_read = imu_i2c_wr_rd,
        .delay_ms   = HAL_Delay,
        .address    = 0x6A,
        .accel_range = QMI8658A_ACCEL_RANGE_2G,
        .gyro_range  = QMI8658A_GYRO_RANGE_2048DPS,
    };
    qmi8658a_init_with_config(&g_imu, &cfg);
}
```

### SPI

```c
#include "qmi8658a.h"

static int imu_spi_tx(const uint8_t *tx, uint16_t len)
{
    return HAL_SPI_Transmit(&hspi1, (uint8_t *)tx, len, 100) == HAL_OK ? 0 : -1;
}

static int imu_spi_tx_rx(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    return HAL_SPI_TransmitReceive(&hspi1, (uint8_t *)tx, rx, len, 100) == HAL_OK ? 0 : -1;
}

static void imu_cs(uint8_t level)
{
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static qmi8658a_t g_imu;

void imu_spi_init(void)
{
    imu_cs(1);  /* 释放 CS；硬件需保证 CS 为低电平时进入 SPI 模式 */
    qmi8658a_init_spi(&g_imu, imu_spi_tx, imu_spi_tx_rx, imu_cs, HAL_Delay);
}
```

**易错点**：

1. I2C 回调用 **7 位地址**；HAL 需 `(addr << 1)`。
2. SPI **每个寄存器事务**都要 `set_cs(0)…set_cs(1)`；驱动内部已处理，回调不要再长期拉低 CS。
3. SPI 读 WHO_AM_I 应发 `0x80`（`QMI8658A_SPI_ADDR_READ(0x00)`），不是 I2C 的 `[0x00]` 写后读。
4. 板级需配置 SPI Mode 0 或 3；MISO 在 4 线模式下接 SDO/SA0。

## 编入工程

1. Keil 加入 `cbb/qmi8658a/qmi8658a.c`，Include Path 增加 `../cbb/qmi8658a`。
2. 在 `Core/` 或 `Common/` 实现上述回调，勿在 `cbb` 内 `#include "hi2c.h"`。
3. Rebuild 确认无未解析符号。

## 参考

- [QMI8658A Datasheet Rev A](https://files.waveshare.com/upload/5/5f/QMI8658A_Datasheet_Rev_A.pdf)
- CBB 总览：[`cbb/README.md`](../README.md)
