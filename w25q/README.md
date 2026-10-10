# W25Q 族 SPI NOR Flash 驱动

本目录为 **Winbond W25Q 系列（含兼容品）** 的通用 C 驱动，通过 **SPI 回调** 接入任意 MCU/HAL，不依赖 `Common` 或具体工程。典型器件含 W25Q16 / W25Q32 / W25Q64JVSSIQ / W25Q128 等（≤16MB，3 字节地址）。

## 器件说明

标准 SPI NOR：JEDEC ID、页编程（256B）、扇区擦除（4KiB）、读数据。容量由 JEDEC capacity 字节自动识别（`flash_size = 1 << capacity`）。

**本驱动不做**：文件系统、Flash 分区、Dual/Quad SPI、4 字节地址（>16MB）、chip erase 对外 API、日志、FreeRTOS 任务。

## 硬件参数

| 项目 | 值 |
|------|-----|
| 接口 | 标准 SPI（Mode 0 常见） |
| 页大小 | 256 B |
| 扇区大小 | 4 KiB（命令 `0x20`） |
| 容量范围 | JEDEC capacity `0x14`…`0x18` → 1MB…16MB |
| 厂商 ID | Winbond `0xEF`；兼容 Zbit `0x5E` |
| JEDEC | 命令 `0x9F` |

## 设计说明

- **回调注入**：必需 `spi_xfer` / `set_cs` / `delay_ms` / `get_tick_ms`；可选 `read_stream`（大块读可用 DMA）。
- **初始化**：Release Power-down → JEDEC → 容量映射 → 清 Block Protect → `initialized`。
- **`skip_id_check`**：跳过厂商白名单（仍拒绝 `0x00`/`0xFF`，且要求 capacity 合法）。
- 与 `zb25vq16` **独立并存**；本驱动为句柄多实例模型。

## 文件

| 文件 | 说明 |
|------|------|
| `w25q.h` | 类型、回调、API |
| `w25q.c` | 指令协议与几何解析 |
| `README.md` | 本文档 |

## API 概览

### 回调类型

```c
typedef int (*w25q_spi_xfer_t)(const uint8_t *tx, uint8_t *rx, uint16_t len);
typedef void (*w25q_set_cs_t)(uint8_t level);       /* 0=选中，1=释放 */
typedef void (*w25q_delay_ms_t)(uint32_t ms);
typedef uint32_t (*w25q_get_tick_ms_t)(void);
typedef int (*w25q_read_stream_t)(uint8_t *buf, uint32_t len); /* 可选 */
```

`spi_xfer`：CS 由驱动拉低期间调用；`rx` 可为 NULL（只发）。返回 0 成功。

### 返回值 `w25q_status_t`

| 值 | 含义 |
|----|------|
| `W25Q_OK` | 成功 |
| `W25Q_ERR_PARAM` | 参数非法 |
| `W25Q_ERR_NOT_INIT` | 未初始化 |
| `W25Q_ERR_BUS` | SPI 回调失败 |
| `W25Q_ERR_TIMEOUT` | WIP 超时 |
| `W25Q_ERR_ID` | JEDEC / 容量非法 |
| `W25Q_ERR_WEL` | Write Enable 未置位 |

### 主要 API

| 函数 | 说明 |
|------|------|
| `w25q_init` | 绑定回调并完成探测与解锁 |
| `w25q_read_jedec_id` | 读 ID；需已填回调，可不 `initialized` |
| `w25q_get_size` / `w25q_get_sector_count` | init 后有效 |
| `w25q_read` / `w25q_write` / `w25q_erase_sector` | 读写擦 |
| `w25q_is_initialized` | 是否已 init |

## 使用示例（STM32 HAL 示意）

```c
static w25q_t s_flash;

static int board_spi_xfer(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    /* HAL_SPI_TransmitReceive 或 Transmit；rx==NULL 时只发 */
    return 0;
}

static void board_set_cs(uint8_t level)
{
    HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin,
                      level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void board_flash_init(void)
{
    w25q_config_t cfg = {
        .spi_xfer = board_spi_xfer,
        .set_cs = board_set_cs,
        .delay_ms = HAL_Delay,
        .get_tick_ms = HAL_GetTick,
        .read_stream = NULL,
        .skip_id_check = false,
    };
    (void)w25q_init(&s_flash, &cfg);
}
```

## 移植清单

- [ ] 实现四类必需回调；注意 `spi_xfer` 在 CS 已拉低时调用
- [ ] SPI 时钟与 Mode 符合器件手册
- [ ] `init` 后核对 `w25q_get_size()` 与丝印容量一致
- [ ] 写前确保目标扇区已擦除（驱动不自动擦）

## 参考

- Winbond W25Q64JV Datasheet（指令集与 Status Register）
- 同库 `zb25vq16`（固定 2MB、port.h 单例模式，可作对照）
