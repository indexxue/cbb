# nPM1300 PMIC 驱动

本目录为 **Nordic nPM1300** 的通用 C 驱动，通过 I2C 回调接入任意 MCU/HAL，不依赖 `Common` 或具体工程。自研薄驱动（对照 PS / 公开寄存器图），**不** vendor Nordic `npmx` ADK。

## 器件说明

nPM1300 集成 Type-C/VBUS 系统调节、锂电池充电、双 BUCK、双 LDO/Load Switch、Ship 等。本驱动覆盖 **A 范围**：寄存器读写、BUCK、LDO/LS、充电基本项、Ship、VBUSIN0 事件读清。

**不做**：fuel gauge 算法、LED/timer/POF 细配、完整 GPIO 复用、INT ISR（板级接 INT 后调 `event_read`）。

## 硬件参数

| 项目 | 值 |
|------|-----|
| 接口 | I2C（最高约 400 kHz～1 MHz，按 PS） |
| 7 位地址 | `0x6B`（`NPM1300_ADDR_7BIT_DEFAULT`） |
| 寄存器帧 | 16-bit 大端地址 + 8-bit 数据 |
| `init` 探测 | 只读 `BUCKSTATUS`（`0x0434`），**不写**任何轨 |

墨水屏 EVT 板（参考）：BUCK1→`VDD_SoC`；LDO1→触摸；LS2→`VDD_EPD`；`EPD_PWR_EN` / `TOUCH_LDO_EN` 常由 **MCU GPIO** 门控。产品路径优先 GPIO；I2C `ldsw_enable` 作调试/备用，避免双控冲突。

## 设计说明

- **回调注入**：必须提供 `write` + `write_read`；`delay_ms` 可选。
- **7 位地址**：回调参数为 7 位；STM32 HAL 用 `NPM1300_HAL_ADDR_FROM_7BIT(addr)`。
- **电压/电流**：严格表匹配；非法 mV/mA → `NPM1300_ERROR_RANGE`（不就近钳位）。
- **`buck_set_vout_mv`**：先写 NORMVOUT，再置 `BUCKSWCTRLSEL` 软件位（避免先切 SW 时落到复位电压）。
- **改 ICHG**：PS 要求在充电器 **禁用** 时改 `BCHGISET*`，再重新使能才生效；请先 `charger_enable(false)` → `set_ichg_ma` → `charger_enable(true)`。

## 文件

| 文件 | 说明 |
|------|------|
| `npm1300.h` | 类型、API |
| `npm1300.c` | 实现 |

## API 概览

### 返回值

| 值 | 含义 |
|----|------|
| `NPM1300_OK` | 成功 |
| `NPM1300_ERROR_PARAM` | 空指针 / 非法索引 |
| `NPM1300_ERROR_NOT_INIT` | 未 init |
| `NPM1300_ERROR_I2C` | 总线失败 |
| `NPM1300_ERROR_RANGE` | 电压/电流不在支持表 |
| `NPM1300_ERROR_UNSUPPORTED` | 保留 |

### 核心 / BUCK / LDSW / 充电 / Ship

| 函数 | 说明 |
|------|------|
| `npm1300_init` | 探测通信；不改轨 |
| `npm1300_reg_write` / `reg_read` | 原始寄存器 |
| `npm1300_buck_set_vout_mv` | 1.0–3.3 V，步进 0.1 V |
| `npm1300_buck_enable` | SET/CLR 脉冲；**关 BUCK1 可能掉电** |
| `npm1300_ldsw_set_mode` | LDO 或 LoadSW |
| `npm1300_ldsw_set_vout_mv` | LDO 模式电压（同 0.1 V 表） |
| `npm1300_ldsw_enable` | SET/CLR 脉冲 |
| `npm1300_charger_set_ichg_ma` | 32–800 mA，步进 2 mA |
| `npm1300_charger_set_vterm_mv` | 精确档（含 4200） |
| `npm1300_charger_enable` | ENABLE SET/CLR |
| `npm1300_ship_enter` | 进 Ship；之后 I2C 未定义 |
| `npm1300_event_read` / `event_clear` | VBUSIN0 事件组 |

## 使用示例（STM32 HAL）

```c
static int board_i2c_write(uint8_t addr7, const uint8_t *data, uint16_t len)
{
    return (HAL_I2C_Master_Transmit(&hi2c1, NPM1300_HAL_ADDR_FROM_7BIT(addr7),
                                    (uint8_t *)data, len, 100) == HAL_OK) ? 0 : -1;
}

static int board_i2c_write_read(uint8_t addr7, const uint8_t *tx, uint16_t tx_len,
                                uint8_t *rx, uint16_t rx_len)
{
    if (tx_len != 2u) {
        return -1;
    }
    uint16_t mem = ((uint16_t)tx[0] << 8) | tx[1];
    return (HAL_I2C_Mem_Read(&hi2c1, NPM1300_HAL_ADDR_FROM_7BIT(addr7),
                             mem, I2C_MEMADD_SIZE_16BIT, rx, rx_len, 100) == HAL_OK)
               ? 0
               : -1;
}

npm1300_t npm;
npm1300_config_t cfg = {
    .write = board_i2c_write,
    .write_read = board_i2c_write_read,
    .i2c_addr = NPM1300_ADDR_7BIT_DEFAULT,
};
npm1300_init(&npm, &cfg);
npm1300_buck_set_vout_mv(&npm, NPM1300_BUCK1, 3000);
```

## 编入工程 / 上板验证

1. 加入 `npm1300.c` 与 Include Path `cbb/npm1300`
2. 板级实现 I2C 回调；勿在 cbb 内引用 HAL/`Common`
3. 本仓冒烟：`projects/ink_panel`（`i2c21` P1.11/P1.12 → nPM1300；`i2c20` 与 `uart20` 互斥）
4. 产品轨门控与充电策略放板级

## 参考

- Nordic nPM1300 Product Specification（TWI / BUCK / LDSW / CHARGER / SHIP）
- 对照：https://github.com/NordicSemiconductor/npmx（勿整库拷入本目录）
