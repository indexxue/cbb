# CAP1203

Microchip **CAP1203** 三通道电容触摸控制器，I2C，MCU 无关薄驱动。

## 硬件要点

| 项 | 值 |
|----|-----|
| 7-bit 地址 | **0x28**（固定） |
| Product ID | `0xFD` → **0x6D** |
| 本板 | 与 nPM1300 共 I2C；`VDD_Touch` 来自 nPM LDO1；ALERT# → MCU |
| 电极 | CS1/CS2 常用；CS3 可预留 |

STM32 HAL：`DevAddress = CAP1203_HAL_ADDR_FROM_7BIT(0x28)`。

## API

| 函数 | 说明 |
|------|------|
| `cap1203_init` | 只读 Product ID；失败 `ERROR_I2C` / `ERROR_ID` |
| `cap1203_reg_read` / `write` | 原始 8-bit 寄存器 |
| `cap1203_configure` | 通道使能、INT、灵敏度、多触摸互锁 |
| `cap1203_read_touched` | 返回 `CAP1203_CSx` 掩码 |
| `cap1203_clear_int` | 清 Main Control INT（释放 ALERT#） |

`delta_sense`：**0 最灵 … 7 最钝**（写入 Sensitivity Control 的 DELTA_SENSE 字段）。

## 最小示例

```c
cap1203_t cap;
cap1203_config_t cfg = {
    .write = board_i2c_write,
    .write_read = board_i2c_write_read,
    .i2c_addr = 0,
};
cap1203_setup_t setup = {
    .sensor_enable = CAP1203_CS1 | CAP1203_CS2,
    .interrupt_enable = CAP1203_CS1 | CAP1203_CS2,
    .delta_sense = 2, /* 接近默认 */
    .disable_multitouch_block = true,
};

cap1203_init(&cap, &cfg);
cap1203_configure(&cap, &setup);

uint8_t mask;
cap1203_read_touched(&cap, &mask);
cap1203_clear_int(&cap);
```

## 编入工程

将 `cap1203.c` 加入源文件列表，include 本目录。勿在驱动内 `#include` HAL。
