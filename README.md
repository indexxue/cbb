# CBB — 通用器件驱动库

`cbb`（Component Building Blocks）是本仓库的 **芯片/模组级 C 驱动集合**，以 **Git 子模块** 维护，可在 STM32G431 工程、ESP 姊妹仓库等多项目中复用。

设计目标：**只描述器件与寄存器协议**，通过 **回调或薄 port 层** 绑定 MCU 总线与 GPIO，**不依赖** 本仓库的 `Common/`、`Core/` 或具体 Keil 工程。

---

## 目录一览

| 目录 | 器件 | 接口 / 说明 | 详细文档 |
|------|------|-------------|----------|
| `eeprom/` | 外置 EEPROM | I2C/SPI 等（按实现） | 见各 `.h` |
| `ili9341/` | ILI9341 TFT | SPI + 引脚回调 | 见 `ili9341.h` |
| `ina226/` | INA226 电流/功率监视 | I2C 回调 | 见 `ina226.h` |
| `mcp4651/` | MCP4651 数字电位器 | I2C | 见 `mcp4651.h` |
| `mpu6050/` | MPU6050 6 轴 IMU | I2C 回调 | 见 `mpu6050.h` |
| `oled/` | OLED 显示 | 按板级接线 | 见 `oled.h` |
| `qmc5883p/` | QMC5883P 三轴磁力计 | I2C 回调 | [qmc5883p/README.md](qmc5883p/README.md) |
| `qmi8658a/` | QMI8658A 6 轴 IMU | I2C 或 SPI 回调 | [qmi8658a/README.md](qmi8658a/README.md) |
| `st7789/` | ST7789 TFT + 绘图 | SPI/引脚回调；含 `lcd.c` 字库层 | 见 `st7789.h`、`lcd.h` |
| `tb6612/` | TB6612FNG 双路电机 | GPIO/PWM 回调 | 见 `tb6612.h` |
| `ws2812b/` | WS2812B RGB 灯带 | USER 单总线 / SPI 单线 / ESP RMT | [ws2812b/README.md](ws2812b/README.md) |
| `xpt2046/` | XPT2046 电阻触摸 | SPI 等，寄存器级 API | 见 `xpt2046.h` |
| `zb25vq16/` | ZB25VQ16 2MB SPI NOR | `zb25vq16_port.h` 由板级实现 | 见 `zb25vq16.h`；本工程 `Core/Src/ext_flash.c` |

未单独建 README 的目录，以对应 `.h` 文件头注释与数据手册为准；新增器件建议在本表与 **子目录 README** 中同步登记。

---

## 子模块获取

本目录对应子模块（见仓库根 `.gitmodules`）：

```bash
git submodule update --init --recursive
```

更新子模块后请在主工程提交中 **记录子模块 commit**，便于追溯。

---

## 两种接入方式

### 1. 回调注入（多数传感器 / 屏）

典型模式（与 `qmc5883p`、`qmi8658a`、`ina226` 等一致）：

- 驱动内 **不包含** `HAL_I2C_*` / `HAL_SPI_*`；
- 由 `Core/` 或未来 `Bsp/` 提供 `write` / `read` / `write_read` 或 `spi_xfer`；
- 回调返回 `0` 表示成功，非 `0` 表示失败；
- I2C 回调参数中的 `addr` 为 **7 位地址**；STM32 HAL 需 `(addr << 1)`。

### 2. Port 钩子（如外扩 Flash）

`zb25vq16` 通过 `zb25vq16_port.h` 声明 `cs_low` / `xfer` / `delay_ms` 等，由 **`ext_flash.c`** 等板级文件实现，驱动本体只发 NOR 命令序列。

---

## 编入 STM32G431 工程（Keil）

1. **只添加用到的 `.c`**：新建 Group，勿整目录通配，避免未实现 port 的模块参与链接。
2. **Include Path**：追加器件目录，例如 `../cbb/qmi8658a`、`../cbb/st7789`。
3. **板级接线**：在 `Core/Src` 实现回调或 port；引脚名使用 Cube `main.h` 用户标签或 `Common/Inc/board_config.h`，**禁止在 `cbb` 内写死引脚**。
4. **Rebuild** 确认无 `undefined symbol`。

主仓库说明见 [`doc/STM32G431_development_standard.md`](../doc/STM32G431_development_standard.md) 第一、三部分。

### 本工程已接入示例

| 用途 | cbb | 板级 / Common |
|------|-----|----------------|
| 外扩 OTA NOR | `zb25vq16` | `Core/Src/ext_flash.c` |
| 厂测磁力计 | `qmc5883p` | `Common/Src/mag.c` → [doc/STM32G431_mag.md](../doc/STM32G431_mag.md) |

其余目录按产品原理图 **按需编入**，未使用的驱动不必加入 `uvprojx`。

---

## 与 ESP 姊妹仓库

`st7789`、`qmc5883p`、`qmi8658a` 等 API 与 ESP-IDF 侧 **尽量保持同一套结构体与回调签名**；移植到 STM32 时仅替换总线实现与延时。`ws2812b` 通用层用 `transmit` / `spi_write` 回调；ESP 工程另编 `ws2812b_esp32.c`（RMT）。

---

# 开发者注意事项

以下为 **强制或强烈建议** 的约定；违反易导致链接错误、子模块漂移或无法在其它 MCU 复用。

## 依赖与分层

| 规则 | 说明 |
|------|------|
| **禁止** `cbb` → `Common` | 驱动不得 `#include` `board_config.h`、`log.h`、`serial_cmd.h` 等 |
| **禁止** `cbb` → `Core` | 不得依赖 `main.h` 句柄、Cube 生成的 `hi2c*` / `hspi*` |
| **允许** 依赖 | 标准 C 库、`stdint.h` / `stdbool.h`、同目录内本器件 `.h/.c` |
| 板级职责 | 总线、CS、复位、背光、中断引脚由 `Core/`（或未来 `Bsp/`）实现并注入 |

若业务需要日志或产测命令，在 **`Common/` 或 `Core/`** 写薄封装（如 `mag.c`），内部再调用 `cbb` API。

## 代码风格

| 项 | 约定 |
|----|------|
| 目录名 | 小写芯片型号，如 `qmi8658a/`、`qmc5883p/` |
| 文件 | 一对 `<chip>.h` + `<chip>.c`；port 可选 `<chip>_port.h` |
| 命名 | 前缀与目录一致：`qmi8658a_init`、`qmc5883p_read_raw` |
| 状态码 | `typedef enum { XXX_OK = 0, XXX_ERROR_I2C, ... } xxx_status_t` |
| 注释 | 文件头 `@brief` 说明接口类型；复杂寄存器可引用数据手册章节 |
| 日志 | **不要** 使用 `ESP_LOGx`；STM32 侧由调用方 `LOG_*`（`cbb` 内保持静默） |

## 新增或修改驱动时

1. **保持 MCU 无关**：所有 HAL 调用放在板级回调里。
2. **I2C 地址**：回调统一用 7 位地址；文档与宏写清楚（如 `QMI8658A_DEFAULT_I2C_ADDR_SA0_HIGH`）。
3. **SPI 传感器**：在 README 中写明 CS 由谁拉、首字节 R/W 位、Mode 0/3 等（参考 `qmi8658a/README.md`）。
4. **子目录 README**：新器件建议增加 `README.md`（用途、硬件参数、API、STM32 示例、编入工程步骤）。
5. **提交范围**：只提交 `.c/.h/README`；编译产物、Keil `.o/.d/.axf` 等由 `cbb/.gitignore` 排除，勿入库。
6. **Keil 工程**：在主仓库 `MDK-ARM/*.uvprojx` 中按需加 Group，勿假设“全 cbb 默认已链接”。

## 测试与验证

- 上电后优先读 **芯片 ID / WHO_AM_I**，失败返回 `*_ERROR_ID`。
- 厂测或 CLI 放在 `Common/`，通过命令触发一次采样即可（参见 `mag` + `serial_cmd`）。
- 改 ODR、量程后注意数据手册要求的 **掉电或关传感器再配置** 顺序（各驱动 `init` 已封装常见流程，扩展 API 时需对照手册）。

## 常见问题

| 现象 | 排查 |
|------|------|
| `undefined symbol` | 对应 `.c` 未加入 uvprojx；或子模块未 `git submodule update` |
| I2C 无应答 | 7 位地址是否左移 1 位；上拉、时钟、器件供电 |
| SPI 读全 0xFF | CS 时序、Mode 0/3、MOSI/MISO 接反、未实现 `spi_xfer` 内 CS |
| 与 ESP 行为不一致 | 对比姊妹仓库同一 `cbb` commit；确认回调语义一致 |

---

## 相关文档

| 文档 | 内容 |
|------|------|
| [STM32G431 开发规范](../doc/STM32G431_development_standard.md) | 分层、Common、Keil、禁止依赖 |
| [Flash 分区](../doc/STM32G431_flash_partition.md) | 片内 + `zb25vq16` 外扩 |
| [厂测磁力计](../doc/STM32G431_mag.md) | `qmc5883p` + `mag.c` |
| [qmc5883p/README.md](qmc5883p/README.md) | 磁力计驱动说明 |
| [qmi8658a/README.md](qmi8658a/README.md) | 6 轴 IMU（I2C/SPI）说明 |

---

**维护**：`cbb` 独立仓库 [indexxue/cbb](https://github.com/indexxue/cbb)（SSH URL 见 `.gitmodules`）。在本目录修改后请在子模块仓库提交，再在主工程更新子模块指针。
