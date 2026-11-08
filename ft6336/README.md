# FT6336 电容触摸驱动

本目录为 **FocalTech FT6336** 的通用 C 驱动，通过 **I2C** 回调接入任意 MCU/HAL，不依赖 `Common` 或具体工程。

## 器件说明

FT6336 为常见电容触摸控制器，最多 **2 点**，默认 I2C 地址 `0x38`。常与 320×480 TFT（如 ST7796）配套。

**本驱动不做**：手势识别、滤波/去抖算法、INT 中断服务、FreeRTOS 任务。INT 引脚由板级轮询或 ISR 触发后调用本 API。

## 硬件参数（本驱动默认）

| 项目 | 值 |
|------|-----|
| 接口 | I2C |
| 7 位地址 | `0x38`（`cfg.address=0` 时默认） |
| CHIP_ID（`0xA3`） | `0x64`（FT6336）；亦接受 `0x36`/`0x06` |
| 最大触点数 | 2 |
| 默认 panel | 320×480（可配置） |

## 设计说明

- 回调注入：必需 `write_read`（repeated start）；可选 `set_rst` + `delay_ms` 做硬件复位。
- `init`：可选复位 → 读 `CHIP_ID` 校验 → `initialized = true`。
- `read_point`：读第一有效触点；无触点返回 `FT6336_ERROR_NO_POINT`。
- `read_points`：最多填 2 点；`out_count` 为有效点数。
- 坐标映射：`panel_*` → `disp_*`，支持 `swap_xy` / `invert_x` / `invert_y`。

## 文件

| 文件 | 说明 |
|------|------|
| `ft6336.h` | 类型、枚举、API |
| `ft6336.c` | 寄存器协议与读点 |

## API 概览

### 回调类型

```c
typedef int (*ft6336_i2c_write_read_t)(uint8_t addr7,
    const uint8_t *write_data, uint16_t write_len,
    uint8_t *read_data, uint16_t read_len); /* 0 成功，非 0 失败 */
typedef void (*ft6336_pin_t)(int high);
typedef void (*ft6336_delay_ms_t)(uint32_t ms);
```

### 返回值 `ft6336_status_t`

| 值 | 含义 |
|----|------|
| `FT6336_OK` | 成功 |
| `FT6336_ERROR_PARAM` | 参数非法 |
| `FT6336_ERROR_I2C` | 总线失败 |
| `FT6336_ERROR_NOT_INIT` | 未初始化 |
| `FT6336_ERROR_ID` | CHIP_ID 不是 `0x64` |
| `FT6336_ERROR_NO_POINT` | 当前无有效触点 |

### 主要 API

| 函数 | 说明 |
|------|------|
| `ft6336_init` | 绑定回调并校验芯片 ID |
| `ft6336_read_point` | 读第一触点 |
| `ft6336_read_points` | 读最多 2 触点 |
| `ft6336_is_initialized` | 查询是否已 init |

## 使用示例（STM32 HAL）

```c
#include "ft6336.h"

extern I2C_HandleTypeDef hi2c1;

static int tp_write_read(uint8_t addr7,
                         const uint8_t *w, uint16_t wlen,
                         uint8_t *r, uint16_t rlen)
{
    if ((w == NULL) || (wlen != 1U)) {
        return -1;
    }
    return HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(addr7 << 1), w[0],
                            I2C_MEMADD_SIZE_8BIT, r, rlen, 100) == HAL_OK ? 0 : -1;
}

static void tp_rst(int high)
{
    HAL_GPIO_WritePin(TP_RST_GPIO_Port, TP_RST_Pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static ft6336_t g_tp;

void tp_board_init(void)
{
    ft6336_config_t cfg = {
        .write_read = tp_write_read,
        .set_rst = tp_rst,
        .delay_ms = HAL_Delay,
        .address = 0, /* 默认 0x38 */
        .panel_w = 320,
        .panel_h = 480,
        .disp_w = 320,
        .disp_h = 480,
        .swap_xy = false,
        .invert_x = false,
        .invert_y = false,
    };
    (void)ft6336_init(&g_tp, &cfg);
}

void tp_poll(void)
{
    ft6336_point_t pts[FT6336_MAX_POINTS];
    uint8_t n = 0;
    if (ft6336_read_points(&g_tp, pts, FT6336_MAX_POINTS, &n) == FT6336_OK) {
        /* pts[0..n-1] */
    }
}
```

## 编入工程

1. 加入 `ft6336.c`，Include Path 追加 `cbb/ft6336`
2. 板级实现 `write_read`（地址为 **7 位**；HAL 需 `addr << 1`）
3. ESP-IDF：本库已在 `cbb/CMakeLists.txt` 注册

## 注意事项

- 若 CHIP_ID 不是 `0x64`（部分兼容芯），先确认模组型号；勿在驱动内硬改期望值绕过校验，除非确认同源寄存器。
- 触摸与显示方向不一致时，优先调 `swap_xy` / `invert_*`，再对齐 LCD `rotation`。
- INT 低有效时，板级可在下降沿置标志，任务里再调用 `read_points`。
