# PN5180 NFC 前端驱动

本目录为 **NXP PN5180** 的通用 C 驱动，通过 SPI + GPIO 回调接入任意 MCU/HAL，不依赖 `Common` 或具体工程。

## 器件说明

PN5180 是一颗多协议 NFC 射频前端，支持 ISO15693、ISO14443、FeliCa 等模式。主机通过 **SPI Mode 0**（CPOL=0, CPHA=0，最高 7 Mbps）发送 Direct Command，配合 **BUSY** 线完成半双工帧交换。

典型用途：

| 应用 | 说明 |
|------|------|
| ISO15693 标签读写 | ICODE SLIX、ICODE2 等 HF 标签 |
| ISO14443 Type A/B | MIFARE、NTAG 等（需上层协议） |
| 产测 | 读 Firmware Version、RF 场开关、IRQ 状态 |

**本驱动负责**：SPI 主机接口、寄存器/EEPROM 读写、RF 配置加载、RF 场开关、TX/RX 缓冲访问。

**本驱动不负责**：ISO15693/14443 命令组帧、防冲突、NFC Forum 协议栈、FreeRTOS 任务。

## 硬件参数（STM32F205VET6 本工程）

引脚详见 [`doc/io_pin_assignment.md`](../../doc/io_pin_assignment.md) 第 1.2 节。

| 项目 | 值 |
|------|-----|
| 接口 | SPI1，15 MHz（Prescaler=4） |
| SPI 引脚 | SCK PB3、MISO PA6、MOSI PA7 |
| NSS | PA4，软件片选，低有效 |
| BUSY | PB2，输入，内部上拉，**高 = 忙** |
| RST | PC5，低有效 |
| PWR_ON | PC4，天线电源使能 |
| IRQ | PE2，EXTI2 下降沿（由板级/应用处理） |

## 设计说明

- **回调注入**：`spi_tx_rx` / `set_nss` / `read_busy` / `set_rst` / `delay_ms` 由板级实现；`set_pwr_on` 可选。
- **BUSY 时序**（NXP 11.4.1）：NSS 拉低 → SPI 交换 → 等 BUSY 高 → NSS 拉高 → 等 BUSY 低；读响应需第二次 SPI 帧（dummy read）。
- **初始化**：可选上电 → 硬件复位 → 等待 `IDLE_IRQ` → 读 EEPROM Firmware Version 校验通信。
- **RF 预设**：`PN5180_RF_TX_ISO15693_26` / `PN5180_RF_RX_ISO15693_26`（0x0D / 0x8D）对应 ISO15693 26 kHz。

## 文件

| 文件 | 说明 |
|------|------|
| `pn5180.h` | 类型、寄存器/命令宏、API |
| `pn5180.c` | SPI 帧交换与芯片控制 |

## API 概览

### 回调类型

```c
typedef int (*pn5180_spi_tx_rx_t)(const uint8_t *tx, uint8_t *rx, uint16_t len);
typedef void (*pn5180_pin_out_t)(int high);
typedef int (*pn5180_pin_in_t)(void);
typedef void (*pn5180_delay_ms_t)(uint32_t ms);
```

`spi_tx_rx` 约定：

| 调用 | 行为 |
|------|------|
| `tx != NULL`, `rx == NULL` | 仅发送 |
| `tx == NULL`, `rx != NULL` | 仅接收（MOSI 发 0xFF） |
| 两者均非 NULL | 全双工交换 |

### 返回值 `pn5180_status_t`

| 值 | 含义 |
|----|------|
| `PN5180_OK` | 成功 |
| `PN5180_ERROR_PARAM` | 参数非法或必要回调缺失 |
| `PN5180_ERROR_NOT_INIT` | 未 `pn5180_init` |
| `PN5180_ERROR_BUS` | SPI 回调失败 |
| `PN5180_ERROR_BUSY` | BUSY 等待超时 |
| `PN5180_ERROR_TIMEOUT` | IRQ 等待超时 |
| `PN5180_ERROR_ID` | Firmware Version 读回 0x00/0xFF |

### 初始化

| 函数 | 说明 |
|------|------|
| `pn5180_register()` | 保存回调，不访问芯片 |
| `pn5180_init()` | 上电/复位/校验，置 `initialized` |
| `pn5180_reset()` | 低有效 RST 脉冲并清 IRQ |

### 寄存器 / EEPROM / RF

| 函数 | 说明 |
|------|------|
| `pn5180_read_register()` / `pn5180_write_register()` | 32 位小端寄存器 |
| `pn5180_write_register_or_mask()` / `_and_mask()` | 读-改-写 |
| `pn5180_read_eeprom()` / `pn5180_write_eeprom()` | EEPROM 顺序读写 |
| `pn5180_load_rf_config()` | 从 EEPROM 加载 RF 参数 |
| `pn5180_rf_on()` / `pn5180_rf_off()` | RF 场开关 |
| `pn5180_setup_rf()` | 加载配置 + 开 RF + Transceive 模式 |

### 数据与 IRQ

| 函数 | 说明 |
|------|------|
| `pn5180_send_data()` | 写 TX 缓冲并发送 |
| `pn5180_read_data()` | 读 RX 缓冲 |
| `pn5180_get_irq_status()` / `pn5180_clear_irq_status()` | IRQ 寄存器 |
| `pn5180_wait_irq()` | 阻塞等待指定位 |
| `pn5180_get_rx_length()` | 从 RX_STATUS 取接收长度 |

## 使用示例（STM32 HAL）

板级文件（如 `Common/Src/pn5180_port.c`）中实现回调，**不要**在 `cbb/pn5180` 内 `#include main.h`。

```c
#include "pn5180.h"
#include "spi.h"
#include "board.h"

extern SPI_HandleTypeDef hspi1;

static int pn5180_spi_tx_rx(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    static uint8_t dummy_tx[512];
    static uint8_t dummy_rx[512];

    if (len > sizeof(dummy_tx))
    {
        return -1;
    }

    if (tx != NULL && rx != NULL)
    {
        return (HAL_SPI_TransmitReceive(&hspi1, (uint8_t *)tx, rx, len, 100) == HAL_OK) ? 0 : -1;
    }
    if (tx != NULL)
    {
        return (HAL_SPI_Transmit(&hspi1, (uint8_t *)tx, len, 100) == HAL_OK) ? 0 : -1;
    }
    if (rx != NULL)
    {
        memset(dummy_tx, 0xFF, len);
        return (HAL_SPI_TransmitReceive(&hspi1, dummy_tx, rx, len, 100) == HAL_OK) ? 0 : -1;
    }
    return -1;
}

static void pn5180_set_nss(int high)
{
    HAL_GPIO_WritePin(PN5180_NSS_GPIO_Port, PN5180_NSS_Pin,
                      high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static int pn5180_read_busy(void)
{
    return (HAL_GPIO_ReadPin(PN5180_BUSY_GPIO_Port, PN5180_BUSY_Pin) == GPIO_PIN_SET) ? 1 : 0;
}

static void pn5180_set_rst(int high)
{
    HAL_GPIO_WritePin(PN5180_RST_GPIO_Port, PN5180_RST_Pin,
                      high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void pn5180_set_pwr_on(int high)
{
    HAL_GPIO_WritePin(PN5180_PWR_ON_GPIO_Port, PN5180_PWR_ON_Pin,
                      high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static pn5180_t g_pn5180;

void board_pn5180_init(void)
{
    pn5180_config_t cfg = PN5180_CONFIG_DEFAULT;
    cfg.spi_tx_rx        = pn5180_spi_tx_rx;
    cfg.set_nss          = pn5180_set_nss;
    cfg.read_busy        = pn5180_read_busy;
    cfg.set_rst          = pn5180_set_rst;
    cfg.set_pwr_on       = pn5180_set_pwr_on;
    cfg.delay_ms         = HAL_Delay;

    pn5180_register(&g_pn5180, &cfg);
    pn5180_init(&g_pn5180);

    /* ISO15693 26 kHz 示例 */
    pn5180_setup_rf(&g_pn5180, PN5180_RF_TX_ISO15693_26, PN5180_RF_RX_ISO15693_26);
}
```

## 编入工程

1. Keil 新建 Group `cbb/pn5180`，加入 `pn5180.c`。
2. Include Path 追加 `../cbb/pn5180`。
3. 在 `Common/` 实现 SPI/GPIO 回调（见上例）。
4. **Rebuild**，确认无未解析符号。

ISO15693 Inventory 等应用层命令请在 `Common/` 封装，调用 `pn5180_send_data` / `pn5180_read_data` 等本驱动 API。

## 参考

- [NXP PN5180 datasheet](https://www.nxp.com/docs/en/data-sheet/PN5180A0XX_C3_C4.pdf)
- [NXP Host Interface Command List](https://docs.nxp.com/bundle/PN5180A0XX_C3_C4/page/topics/host_interface_command_list.html)
- 本工程 IO：[`doc/io_pin_assignment.md`](../../doc/io_pin_assignment.md)
