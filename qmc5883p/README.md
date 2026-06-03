# QMC5883P 三轴磁力计驱动

本目录为 **QST QMC5883P** 的通用 C 驱动，通过 I2C 回调接入任意 MCU/HAL，不依赖 `Common` 或具体工程。

## 磁力计是做什么的？

QMC5883P 测量周围 **静磁场** 在 X、Y、Z 三个方向上的分量，输出为 16 位有符号数（单位：LSB，可按量程换算为高斯 Gauss）。

典型用途包括：

| 应用 | 说明 |
|------|------|
| **电子罗盘 / 航向** | 结合加速度计做倾斜补偿后，由水平面磁场推算 **方位角（heading）** |
| **地图旋转** | 平板、手持设备根据朝向旋转 UI |
| **门磁 / 接近** | 检测磁场变化（需结合阈值与标定） |
| **产测** | 确认 I2C 通信、芯片 ID、三轴读数非异常值 |

注意：附近铁磁材料、电机、大电流走线会 **干扰读数**；PCB 布局需按数据手册避开磁敏感区下方的铜皮。本驱动只负责 **读寄存器与换算**，不做罗盘融合或硬铁/软铁校准。

## 硬件参数（本工程默认）

| 项目 | 值 |
|------|-----|
| 接口 | I2C（100 kHz / 400 kHz） |
| 7 位地址 | `0x2C`（`QMC5883P_DEFAULT_I2C_ADDR`） |
| CHIPID（寄存器 `0x00`） | `0x80` |
| 量程（可配置） | ±2 / ±8 / ±12 / ±30 Gauss |


## 设计说明

- **回调注入**：`write` / `read` 或 `write_read`（推荐 Mem 读，一次 repeated start）由板级实现。
- **7 位地址**：回调参数 `addr` 为 7 位地址（如 `0x2C`）；STM32 HAL 需自行 `(addr << 1)`。
- **初始化**：校验 CHIPID → 写 `0x29 = 0x06`（轴符号）→ 配置 CTRL2 / CTRL1。
- **默认配置**（`qmc5883p_init()`）：Normal 模式、100 Hz、±8 G、OSR/DSR=8、Set/Reset 开启。

## 文件

| 文件 | 说明 |
|------|------|
| `qmc5883p.h` | 类型、枚举、API 声明 |
| `qmc5883p.c` | 寄存器读写与初始化 |

## API 概览

### 回调类型

```c
typedef int (*qmc5883p_i2c_write_t)(uint8_t addr, const uint8_t *data, uint16_t len);
typedef int (*qmc5883p_i2c_read_t)(uint8_t addr, uint8_t *data, uint16_t len);
typedef int (*qmc5883p_i2c_write_read_t)(uint8_t addr,
                                         const uint8_t *write_data, uint16_t write_len,
                                         uint8_t *read_data, uint16_t read_len);
typedef void (*qmc5883p_delay_ms_t)(uint32_t ms);
```

`write` 与 `write_read` 至少提供其一；读寄存器时优先使用 `write_read`。

### 返回值 `qmc5883p_status_t`

| 值 | 含义 |
|----|------|
| `QMC5883P_OK` | 成功 |
| `QMC5883P_ERROR_I2C` | 总线失败 |
| `QMC5883P_ERROR_NOT_INIT` | 未初始化或回调缺失 |
| `QMC5883P_ERROR_PARAM` | 参数非法 |
| `QMC5883P_ERROR_ID` | CHIPID 不是 `0x80` |

### 初始化

| 函数 | 说明 |
|------|------|
| `qmc5883p_init()` | 使用默认地址 `0x2C` 与默认测量配置 |
| `qmc5883p_init_with_config()` | 自定义 `qmc5883p_config_t`（模式、ODR、量程等） |

### 运行中配置

| 函数 | 说明 |
|------|------|
| `qmc5883p_set_mode()` | Suspend / Normal / Single / Continuous |
| `qmc5883p_set_odr()` | 10 / 50 / 100 / 200 Hz |
| `qmc5883p_set_range()` | ±2 / ±8 / ±12 / ±30 G |
| `qmc5883p_soft_reset()` | 软复位，需重新 `init` |

### 数据与状态

| 函数 | 说明 |
|------|------|
| `qmc5883p_read_raw()` | 读三轴原始值 `int16_t`（X/Y/Z，LSB 在前） |
| `qmc5883p_read_gauss()` | 按当前量程换算为 Gauss（`float`） |
| `qmc5883p_data_ready()` | 状态寄存器 DRDY |
| `qmc5883p_overflow()` | 任一路径溢出标志 OVFL |

### 量程与 LSB/Gauss（`read_gauss`）

| `qmc5883p_range_t` | 满量程 | LSB/Gauss |
|--------------------|--------|-----------|
| `QMC5883P_RANGE_30G` | ±30 G | 1000 |
| `QMC5883P_RANGE_12G` | ±12 G | 2500 |
| `QMC5883P_RANGE_8G`  | ±8 G  | 3750 |
| `QMC5883P_RANGE_2G`  | ±2 G  | 15000 |

## 使用示例（STM32 HAL）

```c
#include "qmc5883p.h"
#include "i2c.h"

static qmc5883p_t mag;

static uint16_t mag_hal_addr(uint8_t addr_7bit)
{
    return (uint16_t)((uint16_t)addr_7bit << 1);
}

static int mag_i2c_write(uint8_t addr, const uint8_t *data, uint16_t len)
{
    return (HAL_I2C_Master_Transmit(&hi2c2, mag_hal_addr(addr),
                                    (uint8_t *)data, len, 100) == HAL_OK) ? 0 : -1;
}

static int mag_i2c_write_read(uint8_t addr, const uint8_t *tx, uint16_t tx_len,
                              uint8_t *rx, uint16_t rx_len)
{
    if (tx_len != 1u) return -1;
    return (HAL_I2C_Mem_Read(&hi2c2, mag_hal_addr(addr), tx[0],
                             I2C_MEMADD_SIZE_8BIT, rx, rx_len, 100) == HAL_OK) ? 0 : -1;
}

void app_mag_init(void)
{
    qmc5883p_config_t cfg = {
        .write      = mag_i2c_write,
        .read       = NULL,
        .write_read = mag_i2c_write_read,
        .delay_ms   = HAL_Delay,
        .address    = QMC5883P_DEFAULT_I2C_ADDR,
        .mode       = QMC5883P_MODE_NORMAL,
        .odr        = QMC5883P_ODR_100HZ,
        .osr        = QMC5883P_OSR_8,
        .dsr        = QMC5883P_DSR_8,
        .range      = QMC5883P_RANGE_8G,
        .setreset   = QMC5883P_SETRESET_ON,
    };

    if (qmc5883p_init_with_config(&mag, &cfg) != QMC5883P_OK) {
        /* 处理错误 */
    }
}

void app_mag_read(void)
{
    bool ready = false;
    int16_t mx, my, mz;

    (void)qmc5883p_data_ready(&mag, &ready);
    if (qmc5883p_read_raw(&mag, &mx, &my, &mz) == QMC5883P_OK) {
        /* 使用 mx, my, mz；做罗盘时需校准与倾角补偿 */
    }
}
```

## 编入工程

1. 将 `qmc5883p.c` 加入工程，头文件路径增加 `cbb/qmc5883p`。
2. 在板级实现 I2C 回调（见上例）。
3. **不要** 在 `cbb` 内引用 `Common`（见 `doc/STM32G431_development_standard.md`）。

厂测（Factory）通过 `Common/Src/mag.c` 接入：USB 就绪后 `mag_init()`，串口命令 `mag` 触发单次采样。详见 `doc/STM32G431_mag.md`。

## 参考

- [QMC5883P 数据手册](https://www.qstcorp.com/upload/pdf/202512/13-52-19%20QMC5883P%20Datasheet%20Rev.E.pdf)（寄存器 `0x0A` / `0x0B`、Normal 模式示例）
