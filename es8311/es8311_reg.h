/**
 * @file es8311_reg.h
 * @brief ES8311 寄存器地址（与 Everest ES8311 数据手册一致）。
 */

#ifndef ES8311_REG_H
#define ES8311_REG_H

#ifdef __cplusplus
extern "C" {
#endif

#define ES8311_RESET_REG00           0x00U
#define ES8311_CLK_MANAGER_REG01     0x01U
#define ES8311_CLK_MANAGER_REG02     0x02U
#define ES8311_CLK_MANAGER_REG03     0x03U
#define ES8311_CLK_MANAGER_REG04     0x04U
#define ES8311_CLK_MANAGER_REG05     0x05U
#define ES8311_CLK_MANAGER_REG06     0x06U
#define ES8311_CLK_MANAGER_REG07     0x07U
#define ES8311_CLK_MANAGER_REG08     0x08U
#define ES8311_SDPIN_REG09           0x09U
#define ES8311_SDPOUT_REG0A          0x0AU
#define ES8311_SYSTEM_REG0B          0x0BU
#define ES8311_SYSTEM_REG0C          0x0CU
#define ES8311_SYSTEM_REG0D          0x0DU
#define ES8311_SYSTEM_REG0E          0x0EU
#define ES8311_SYSTEM_REG10          0x10U
#define ES8311_SYSTEM_REG11          0x11U
#define ES8311_SYSTEM_REG12          0x12U
#define ES8311_SYSTEM_REG13          0x13U
#define ES8311_SYSTEM_REG14          0x14U
#define ES8311_ADC_REG15             0x15U
#define ES8311_ADC_REG16             0x16U
#define ES8311_ADC_REG17             0x17U
#define ES8311_ADC_REG1B             0x1BU
#define ES8311_ADC_REG1C             0x1CU
#define ES8311_DAC_REG31             0x31U
#define ES8311_DAC_REG32             0x32U
#define ES8311_DAC_REG37             0x37U
#define ES8311_GPIO_REG44            0x44U
#define ES8311_GP_REG45              0x45U
#define ES8311_CHD1_REGFD            0xFDU
#define ES8311_CHD2_REGFE            0xFEU

#define ES8311_MCLK_DIV_DEFAULT      (256U)

#ifdef __cplusplus
}
#endif

#endif /* ES8311_REG_H */
