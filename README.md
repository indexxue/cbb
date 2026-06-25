# CBB 通用器件驱动库

`cbb`（Common Building Blocks）存放 **与 MCU / 工程无关** 的器件驱动，通过 **回调注入** 绑定 HAL、ESP-IDF 或其它总线实现。本目录为 **Git 子模块**，可在 STM32、ESP32 等姊妹仓库间复用。

与主工程分层关系见 [`doc/STM32G431_development_standard.md`](../doc/STM32G431_development_standard.md) 第 1.2 节。

---

## 目录

| 子目录 | 器件 | 接口 | 说明 |
|--------|------|------|------|
| `ds3231` | DS3231 | I2C | 高精度 RTC，BCD 时间寄存器 |
| `epd_2in9` | 2.9 寸电子墨水屏 | SPI | 128×296，SSD1606 / SSD1680 |
| `eeprom` | 外扩 EEPROM | I2C / SPI | 非易失存储 |
| `ili9341` | ILI9341 TFT | SPI | 240×320 屏 |
| `ina226` | INA226 | I2C | 电流/功率监视 |
| `mcp4651` | MCP4651 | SPI | 数字电位器 |
| `mpu6050` | MPU6050 | I2C | 六轴 IMU（旧款） |
| `oled` | SSD1306 等 OLED | I2C / SPI | 单色 OLED |
| `qmc5883p` | QMC5883P | I2C | 三轴磁力计 |
| `qmi8658a` | QMI8658A | I2C / SPI | 六轴 IMU |
| `st7789` | ST7789 | SPI | 240×135 TFT |
| `tb6612` | TB6612FNG | GPIO / PWM | 双路电机驱动 |
| `ws2812b` | WS2812B | 单线时序 | RGB 灯带（ESP RMT 实现） |
| `xpt2046` | XPT2046 | SPI | 电阻触摸屏 |
| `zb25vq16` | ZB25VQ16 | SPI | 外扩 NOR Flash |

各器件详细说明见对应子目录下的 `README.md`（新建驱动时按本文 **第四节** 模板编写）。

---

## 一、设计原则

### 1.1 平台无关

- 驱动 **不得** `#include` 工程内的 `Common/`、`main.h`、具体 HAL 头文件（少数历史模块如 `ws2812b` 绑定 ESP-IDF 的除外，新建驱动应避免）。
- 所有总线、GPIO、延时由 **板级通过回调** 提供；驱动只关心寄存器协议与数据换算。
- 使用 `<stdint.h>` / `<stdbool.h>`，禁止依赖 `int` 宽度假设。

### 1.2 回调注入

按接口类型选择回调组合：

| 接口 | 典型回调 | 说明 |
|------|----------|------|
| I2C | `write` / `read` / `write_read` / `delay_ms` | **7 位从机地址**；`write_read` 优先（一次 repeated start） |
| SPI 屏 / Flash | `spi_tx`（及可选 `spi_tx_rx`）/ `set_cs` / `set_dc` / `delay_ms` | 字节流由板级 DMA 或阻塞 SPI 实现 |
| GPIO 类 | `gpio_set` / `pwm_set` / `get_tick_ms` | 引脚编号用驱动内定义的 `PIN_*` 常量映射 |
| 单线时序 | 平台专用（如 RMT） | 需在 README 中注明目标平台 |

**I2C 地址约定**：回调参数 `addr` 为 **7 位地址**（如 `0x2C`）。STM32 HAL 的 `DevAddress` 需板级 `(addr << 1)`，可在头文件提供辅助宏（参考 `ina226.h` 的 `INA226_HAL_ADDR_FROM_7BIT`）。

**回调返回值**：成功 `0`，失败非 `0`（通常 `-1`），与 HAL 成功/失败解耦。

### 1.3 设备句柄与状态

标准模式：

```c
typedef struct {
    /* 回调指针 + 配置项 */
    bool initialized;
} xxx_t;

typedef enum {
    XXX_OK = 0,
    XXX_ERROR_I2C,
    XXX_ERROR_NOT_INIT,
    XXX_ERROR_PARAM,
    XXX_ERROR_ID,   /* WHO_AM_I / CHIPID 校验失败 */
} xxx_status_t;
```

- `xxx_config_t`：初始化入参（含回调与默认量程等）。
- `xxx_t`：运行时句柄；`init` 成功后置 `initialized = true`。
- 对外 API 首先校验 `dev != NULL`、必要回调非空、已初始化。

### 1.4 命名与文件布局

| 项 | 规范 |
|----|------|
| 目录名 | 小写芯片型号，如 `qmi8658a/` |
| 源文件 | `<芯片>.h` + `<芯片>.c`；字体/常量可另增 `*font.h` |
| 前缀 | 全部 API、类型、宏以 `<芯片>_` 为前缀 |
| 头文件守卫 | `#ifndef XXX_H` / `#define XXX_H` |
| C++ | 头文件内 `#ifdef __cplusplus` / `extern "C"` |

### 1.5 职责边界

- **驱动负责**：寄存器读写、初始化序列、量程/模式配置、原始值与物理量换算。
- **驱动不负责**：传感器融合、罗盘校准、UI 绘制、FreeRTOS 任务、日志输出。
- 板级 / `Common/` 负责：实例化句柄、接 HAL、产测命令、业务逻辑。

---

## 二、代码开发规范

### 2.1 头文件（`.h`）

1. 文件头 Doxygen：`@file`、`@brief`（中英文均可，说明接口类型与是否 MCU 无关）。
2. 公开 API 均声明在 `.h`；寄存器地址、位域、默认值放在 `.c` 为 `static` 或 `#define`。
3. 配置项用 `enum` 或具名宏，避免魔法数字。
4. 每个对外函数一行 `@brief` 注释（复杂行为可展开）。

### 2.2 源文件（`.c`）

1. 内部辅助函数：`static`，命名清晰（如 `xxx_read_regs`、`xxx_write_reg`）。
2. 读寄存器统一入口：优先 `write_read`；否则 `write` + `read` 分开调用。
3. `init` 流程建议：参数检查 → 保存回调 → 读 ID 校验 → 软复位（若需要）→ 写默认配置 → `initialized = true`。
4. 不在驱动内调用 `printf` / `LOG_*` / `ESP_LOGx`。
5. 函数长度建议 ≤ 80 行；复杂初始化拆成 static 步骤函数。

### 2.3 错误处理

| 场景 | 返回值 |
|------|--------|
| 指针为 NULL、长度为 0 | `XXX_ERROR_PARAM` |
| 未 init 或关键回调缺失 | `XXX_ERROR_NOT_INIT` |
| 总线回调失败 | `XXX_ERROR_I2C`（SPI 类可命名为 `XXX_ERROR_BUS`） |
| 芯片 ID 不匹配 | `XXX_ERROR_ID` |

禁止静默吞掉错误；禁止在失败路径仍置 `initialized = true`。

### 2.4 依赖与移植

从 ESP32 或其它平台迁入时：

1. 删除 `driver/i2c.h`、`esp_err.h` 等平台头文件。
2. 将 `esp_err_t` 改为本库 `xxx_status_t`。
3. Flash 分区、引脚宏改为回调或 `config` 字段。
4. 在目标工程板级实现回调并跑通 `init` + 一次读数。

### 2.5 提交前检查

- [ ] 未引用 `Common/` 或产品工程私有头文件
- [ ] 7 位 I2C 地址与数据手册一致，README 已说明
- [ ] `init` 含 WHO_AM_I / CHIPID 校验
- [ ] 已添加子目录 `README.md`
- [ ] 已在姊妹仓库 Keil / CMake 中验证编译（仅加入用到的 `.c`）

---

## 三、编入主工程（STM32 Keil）

1. 克隆子模块：`git submodule update --init --recursive`
2. 在 `MDK-ARM/STM32G431CBT6.uvprojx` 新建 **Group**（如 `cbb/qmi8658a`），只加入需要的 `.c`。
3. **Include Path** 追加器件目录，例如 `../cbb/qmi8658a`。
4. 在 `Core/` 或 `Common/` 中编写板级适配（I2C/SPI 回调），**不要**改 `cbb` 内源码去包含 `hi2c2`。
5. **Rebuild**，确认无未解析符号；关注 Code 是否仍低于 APP-A 96KB 限制。

产测、串口命令等应在 `Common/Src` 封装（如 `mag.c`），通过已初始化的 `cbb` 句柄调用，而非在 `cbb` 内注册命令。

---

## 四、单器件 README 编写指引

每个 `cbb/<芯片>/` **必须** 提供 `README.md`，便于跨项目复用与 Code Review。推荐结构如下（可复制为模板）。

### 4.1 文档结构模板

```markdown
# <芯片型号> <中文简短描述>

本目录为 **<厂商> <型号>** 的通用 C 驱动，通过 <I2C/SPI/GPIO> 回调接入任意 MCU/HAL，不依赖 Common 或具体工程。

## 器件说明

- 一两段话：测量/控制什么、典型应用场景（表格可选）。
- **明确驱动不做什么**（如：不做融合算法、不做文件系统）。

## 硬件参数（本工程默认）

| 项目 | 值 |
|------|-----|
| 接口 | … |
| 地址 / 片选 | … |
| ID 寄存器 | … |
| 量程 / 关键电气 | … |

## 设计说明

- 回调注入方式、地址位宽、初始化步骤摘要、默认配置。

## 文件

| 文件 | 说明 |
|------|------|
| `xxx.h` | 类型、枚举、API |
| `xxx.c` | 实现 |

## API 概览

### 回调类型
（列出 typedef，说明 write / write_read 至少其一）

### 返回值
（表格枚举各 status）

### 初始化 / 配置 / 数据读取
（按功能分组列表）

### 换算关系（若有）
（LSB → 物理单位表格）

## 使用示例（STM32 HAL）

（完整可编译风格的板级回调 + init + 读数示例）

## 编入工程

1. 加入 `.c` 与 Include Path
2. 板级实现回调
3. 勿在 cbb 内引用 Common

## 参考

- 数据手册链接
- 相关 `doc/` 文档（若主工程有专文）
```

### 4.2 写作要点

| 要点 | 说明 |
|------|------|
| 读者 | 未看过源码的嵌入式工程师；先懂「干什么用」，再看 API |
| 示例 | 必须含 **HAL 地址左移** 或 SPI 片选时序等易错点 |
| 默认配置 | 写清 `xxx_init()` 与 `xxx_init_with_config()` 的差异 |
| 边界 | 干扰、布局、量程溢出等产品级注意单独一小节 |
| 语言 | 中文为主；API 名、寄存器、宏保持英文与代码一致 |
| 篇幅 | 单器件 README 一般 150～250 行；过长的寄存器表放数据手册链接 |

### 4.3 参考范例

本库中 **`qmc5883p/README.md`**（若已提交）为较完整范例，涵盖器件说明、API 表、STM32 HAL 示例与编入工程步骤。新建驱动时请与之保持同级详细度。

---

## 五、新建驱动工作流

```
1. 在 cbb/ 下新建 <芯片>/ 目录
2. 编写 <芯片>.h / <芯片>.c（遵循第二节规范）
3. 按第四节模板编写 README.md
4. 在板级工程实现回调，验证 init + 基本读写
5. 主工程 Keil 加 Group 与 Include Path
6. 子模块仓库提交；主工程更新 submodule 指针
```

---

## 六、相关文档

| 文档 | 路径 |
|------|------|
| STM32 工程开发规范 | [`doc/STM32G431_development_standard.md`](../doc/STM32G431_development_standard.md) |
| Flash 分区 | [`doc/STM32G431_flash_partition.md`](../doc/STM32G431_flash_partition.md) |
| 板级开关 | [`Common/Inc/board_config.h`](../Common/Inc/board_config.h) |

---

## 修订记录

| 日期 | 说明 |
|------|------|
| 2026-06-04 | 初版：CBB 总览、开发规范、单器件 README 模板与编入工程说明 |
