/**
 * @file    fd6288q.h
 * @brief   Fortior FD6288Q 三相半桥栅极驱动（HIN/LIN，无 MCU 依赖）。
 *
 * 芯片无寄存器总线：逻辑输入 HIN1..3 / LIN1..3 同相驱动 HO/LO。
 * 内置直通防止与死区（典型 ~200 ns）。
 * 本驱动可选 **软件死区**：换相/改桥臂时先 FLOAT，再 delay_ns，最后施加新状态。
 * 板级仍应配置 TIM BDTR 硬件死区（与软件叠加）。
 *
 * 通过回调注入绑定 TIM 互补 PWM / GPIO；本驱动不做 FOC / 传感器换相。
 *
 * @see Fortior FD6288T&Q datasheet
 */

#ifndef FD6288Q_H
#define FD6288Q_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 三相桥臂索引：逻辑相 U/V/W（对应芯片 HIN1/2/3、LIN1/2/3） */
#define FD6288Q_PHASE_U  0u
#define FD6288Q_PHASE_V  1u
#define FD6288Q_PHASE_W  2u
#define FD6288Q_PHASE_N  3u

/** 占空比满量程（千分比） */
#define FD6288Q_DUTY_MAX 1000u

/** 六步换相步数 */
#define FD6288Q_STEP_N   6u

/**
 * 可选板级 GPIO 逻辑脚（由 gpio_set / gpio_get 解释）。
 * 芯片本身无 EN 脚；若原理图有使能 MOSFET / nFAULT，用这两路扩展。
 */
#define FD6288Q_PIN_ENABLE  0u
#define FD6288Q_PIN_FAULT   1u

/**
 * 单桥臂驱动模式（映射到 HIN/LIN）。
 * FLOAT：HIN=0 LIN=0（高阻，用于六步未激励相 / 软件死区过渡）
 * LOW：  HIN=0 LIN=1（下管导通）
 * HIGH： HIN=1 LIN=0（上管导通）
 * PWM：  互补 PWM，占空比 duty_permille 作用在高侧（板级决定 CH/CHN）
 */
typedef enum
{
    FD6288Q_LEG_FLOAT = 0,
    FD6288Q_LEG_LOW,
    FD6288Q_LEG_HIGH,
    FD6288Q_LEG_PWM
} fd6288q_leg_mode_t;

typedef enum
{
    FD6288Q_OK = 0,
    FD6288Q_ERROR_PARAM,
    FD6288Q_ERROR_NOT_INIT,
    FD6288Q_ERROR_FAULT
} fd6288q_status_t;

/**
 * @brief 应用一相桥臂状态。
 * @param phase         FD6288Q_PHASE_U/V/W（逻辑相，非 TIM 通道号）
 * @param mode          桥臂模式
 * @param duty_permille 仅 LEG_PWM 有效，0..FD6288Q_DUTY_MAX
 */
typedef void (*fd6288q_leg_apply_t)(uint8_t phase,
                                    fd6288q_leg_mode_t mode,
                                    uint16_t duty_permille);

/** 可选：写 ENABLE 等 GPIO；level 0/1 */
typedef void (*fd6288q_gpio_set_t)(uint8_t pin_id, uint8_t level);

/** 可选：读 FAULT；返回 0/1，负值表示不可用 */
typedef int (*fd6288q_gpio_get_t)(uint8_t pin_id);

/**
 * @brief 忙等延时（软件死区用），单位 ns。
 *        板级可用 DWT / 循环实现；分辨率不足时可向上取整。
 */
typedef void (*fd6288q_delay_ns_t)(uint32_t ns);

typedef struct
{
    fd6288q_leg_apply_t leg_apply;   /**< 必填 */
    fd6288q_gpio_set_t  gpio_set;    /**< 可选 */
    fd6288q_gpio_get_t  gpio_get;    /**< 可选 */
    fd6288q_delay_ns_t  delay_ns;    /**< 软件死区；deadtime_ns>0 时建议提供 */
    uint32_t            deadtime_ns; /**< 软件死区，0=关闭；本板推荐 500 */
    bool                enable_active_high;
    bool                fault_active_low;
} fd6288q_config_t;

typedef struct
{
    fd6288q_leg_apply_t leg_apply;
    fd6288q_gpio_set_t  gpio_set;
    fd6288q_gpio_get_t  gpio_get;
    fd6288q_delay_ns_t  delay_ns;
    uint32_t            deadtime_ns;
    bool                enable_active_high;
    bool                fault_active_low;
    bool                initialized;
    bool                enabled;
    uint8_t             step;
    uint16_t            duty_permille;
    fd6288q_leg_mode_t  leg_mode[FD6288Q_PHASE_N];
} fd6288q_t;

/**
 * @brief 注册回调并进入安全态（三相 FLOAT，ENABLE 关）。
 */
fd6288q_status_t fd6288q_init(fd6288q_t *dev, const fd6288q_config_t *cfg);

/**
 * @brief 使能/关闭板级 ENABLE（无 gpio_set 时仅更新内部标志）。
 *        enable=false 时强制三相 FLOAT。
 */
fd6288q_status_t fd6288q_enable(fd6288q_t *dev, bool enable);

/** @brief 三相 FLOAT（滑行/关断输出） */
fd6288q_status_t fd6288q_coast(fd6288q_t *dev);

/** @brief 三相下管导通（低压侧短路制动）；含软件死区过渡 */
fd6288q_status_t fd6288q_brake_low(fd6288q_t *dev);

/** @brief 设置单相桥臂（模式变化时插入软件死区） */
fd6288q_status_t fd6288q_set_leg(fd6288q_t *dev,
                                 uint8_t phase,
                                 fd6288q_leg_mode_t mode,
                                 uint16_t duty_permille);

/**
 * @brief 六步换相：step∈[0,5]，PWM 相占空比 duty_permille。
 *        换步时先将需切换的相 FLOAT + 软件死区，再施加新步表。
 */
fd6288q_status_t fd6288q_set_step(fd6288q_t *dev,
                                  uint8_t step,
                                  uint16_t duty_permille);

/** @brief 仅改当前步的占空比（步序不变；无换相死区） */
fd6288q_status_t fd6288q_set_duty(fd6288q_t *dev, uint16_t duty_permille);

/** @brief 紧急关断：DISABLE + coast（不等待死区） */
fd6288q_status_t fd6288q_emergency_stop(fd6288q_t *dev);

bool fd6288q_is_initialized(const fd6288q_t *dev);
bool fd6288q_is_enabled(const fd6288q_t *dev);
uint8_t fd6288q_get_step(const fd6288q_t *dev);
uint16_t fd6288q_get_duty(const fd6288q_t *dev);
uint32_t fd6288q_get_deadtime_ns(const fd6288q_t *dev);

/**
 * @brief 读取 FAULT（无 gpio_get 时返回 false=无故障）。
 */
fd6288q_status_t fd6288q_fault_get(const fd6288q_t *dev, bool *faulted);

#ifdef __cplusplus
}
#endif

#endif /* FD6288Q_H */
