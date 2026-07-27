/**
 * @file    fd6288q.c
 * @brief   FD6288Q three-phase gate driver (callback-based, MCU-agnostic).
 */

#include "fd6288q.h"

#include <stddef.h>

/*
 * Six-step table (CW electrical; phase = chip HIN1/2/3 order):
 *   step | U     | V     | W
 *   -----+-------+-------+------
 *     0  | PWM   | LOW   | FLOAT
 *     1  | PWM   | FLOAT | LOW
 *     2  | FLOAT | PWM   | LOW
 *     3  | LOW   | PWM   | FLOAT
 *     4  | LOW   | FLOAT | PWM
 *     5  | FLOAT | LOW   | PWM
 */
static const fd6288q_leg_mode_t s_step_tbl[FD6288Q_STEP_N][FD6288Q_PHASE_N] = {
    { FD6288Q_LEG_PWM,   FD6288Q_LEG_LOW,   FD6288Q_LEG_FLOAT },
    { FD6288Q_LEG_PWM,   FD6288Q_LEG_FLOAT, FD6288Q_LEG_LOW   },
    { FD6288Q_LEG_FLOAT, FD6288Q_LEG_PWM,   FD6288Q_LEG_LOW   },
    { FD6288Q_LEG_LOW,   FD6288Q_LEG_PWM,   FD6288Q_LEG_FLOAT },
    { FD6288Q_LEG_LOW,   FD6288Q_LEG_FLOAT, FD6288Q_LEG_PWM   },
    { FD6288Q_LEG_FLOAT, FD6288Q_LEG_LOW,   FD6288Q_LEG_PWM   },
};

static uint16_t clamp_duty(uint16_t duty)
{
    return (duty > FD6288Q_DUTY_MAX) ? FD6288Q_DUTY_MAX : duty;
}

static bool mode_is_drive(fd6288q_leg_mode_t mode)
{
    return (mode == FD6288Q_LEG_LOW) ||
           (mode == FD6288Q_LEG_HIGH) ||
           (mode == FD6288Q_LEG_PWM);
}

static void apply_enable_pin(fd6288q_t *dev, bool enable)
{
    uint8_t level;

    if (dev->gpio_set == NULL)
    {
        return;
    }

    if (dev->enable_active_high)
    {
        level = enable ? 1u : 0u;
    }
    else
    {
        level = enable ? 0u : 1u;
    }
    dev->gpio_set(FD6288Q_PIN_ENABLE, level);
}

static void sw_deadtime_wait(fd6288q_t *dev)
{
    if (dev->deadtime_ns == 0u || dev->delay_ns == NULL)
    {
        return;
    }
    dev->delay_ns(dev->deadtime_ns);
}

static void apply_all_float(fd6288q_t *dev)
{
    uint8_t i;

    for (i = 0u; i < FD6288Q_PHASE_N; i++)
    {
        dev->leg_apply(i, FD6288Q_LEG_FLOAT, 0u);
        dev->leg_mode[i] = FD6288Q_LEG_FLOAT;
    }
}

/**
 * 换相软件死区：任一相旧/新驱动态不同时，先将该相 FLOAT，
 * 全部过渡完后等待 deadtime_ns，再施加目标模式。
 */
static void apply_legs_with_deadtime(fd6288q_t *dev,
                                     const fd6288q_leg_mode_t new_mode[FD6288Q_PHASE_N],
                                     uint16_t duty)
{
    uint8_t i;
    bool need_dt = false;

    for (i = 0u; i < FD6288Q_PHASE_N; i++)
    {
        if (dev->leg_mode[i] == new_mode[i])
        {
            continue;
        }
        if (mode_is_drive(dev->leg_mode[i]) || mode_is_drive(new_mode[i]))
        {
            if (dev->leg_mode[i] != FD6288Q_LEG_FLOAT)
            {
                dev->leg_apply(i, FD6288Q_LEG_FLOAT, 0u);
                dev->leg_mode[i] = FD6288Q_LEG_FLOAT;
            }
            need_dt = true;
        }
    }

    if (need_dt)
    {
        sw_deadtime_wait(dev);
    }

    for (i = 0u; i < FD6288Q_PHASE_N; i++)
    {
        uint16_t d = (new_mode[i] == FD6288Q_LEG_PWM) ? duty : 0u;
        if (dev->leg_mode[i] == new_mode[i] && new_mode[i] != FD6288Q_LEG_PWM)
        {
            continue;
        }
        if (dev->leg_mode[i] == new_mode[i] && new_mode[i] == FD6288Q_LEG_PWM)
        {
            /* 同为 PWM：只刷新占空比 */
            dev->leg_apply(i, FD6288Q_LEG_PWM, d);
            continue;
        }
        dev->leg_apply(i, new_mode[i], d);
        dev->leg_mode[i] = new_mode[i];
    }
}

fd6288q_status_t fd6288q_init(fd6288q_t *dev, const fd6288q_config_t *cfg)
{
    uint8_t i;

    if (dev == NULL || cfg == NULL || cfg->leg_apply == NULL)
    {
        return FD6288Q_ERROR_PARAM;
    }
    if (cfg->deadtime_ns > 0u && cfg->delay_ns == NULL)
    {
        return FD6288Q_ERROR_PARAM;
    }

    dev->leg_apply = cfg->leg_apply;
    dev->gpio_set = cfg->gpio_set;
    dev->gpio_get = cfg->gpio_get;
    dev->delay_ns = cfg->delay_ns;
    dev->deadtime_ns = cfg->deadtime_ns;
    dev->enable_active_high = cfg->enable_active_high;
    dev->fault_active_low = cfg->fault_active_low;
    dev->initialized = false;
    dev->enabled = false;
    dev->step = 0u;
    dev->duty_permille = 0u;

    for (i = 0u; i < FD6288Q_PHASE_N; i++)
    {
        dev->leg_mode[i] = FD6288Q_LEG_FLOAT;
    }

    apply_all_float(dev);
    apply_enable_pin(dev, false);

    dev->initialized = true;
    return FD6288Q_OK;
}

fd6288q_status_t fd6288q_enable(fd6288q_t *dev, bool enable)
{
    if (dev == NULL || !dev->initialized)
    {
        return FD6288Q_ERROR_NOT_INIT;
    }

    if (!enable)
    {
        apply_all_float(dev);
        apply_enable_pin(dev, false);
        dev->enabled = false;
        dev->duty_permille = 0u;
        return FD6288Q_OK;
    }

    apply_enable_pin(dev, true);
    dev->enabled = true;
    return FD6288Q_OK;
}

fd6288q_status_t fd6288q_coast(fd6288q_t *dev)
{
    if (dev == NULL || !dev->initialized)
    {
        return FD6288Q_ERROR_NOT_INIT;
    }

    apply_all_float(dev);
    dev->duty_permille = 0u;
    return FD6288Q_OK;
}

fd6288q_status_t fd6288q_brake_low(fd6288q_t *dev)
{
    fd6288q_leg_mode_t modes[FD6288Q_PHASE_N];
    uint8_t i;

    if (dev == NULL || !dev->initialized)
    {
        return FD6288Q_ERROR_NOT_INIT;
    }

    for (i = 0u; i < FD6288Q_PHASE_N; i++)
    {
        modes[i] = FD6288Q_LEG_LOW;
    }
    apply_legs_with_deadtime(dev, modes, 0u);
    dev->duty_permille = 0u;
    return FD6288Q_OK;
}

fd6288q_status_t fd6288q_set_leg(fd6288q_t *dev,
                                 uint8_t phase,
                                 fd6288q_leg_mode_t mode,
                                 uint16_t duty_permille)
{
    fd6288q_leg_mode_t modes[FD6288Q_PHASE_N];
    uint8_t i;

    if (dev == NULL || !dev->initialized)
    {
        return FD6288Q_ERROR_NOT_INIT;
    }
    if (phase >= FD6288Q_PHASE_N || mode > FD6288Q_LEG_PWM)
    {
        return FD6288Q_ERROR_PARAM;
    }

    duty_permille = clamp_duty(duty_permille);
    if (mode != FD6288Q_LEG_PWM)
    {
        duty_permille = 0u;
    }

    for (i = 0u; i < FD6288Q_PHASE_N; i++)
    {
        modes[i] = dev->leg_mode[i];
    }
    modes[phase] = mode;
    apply_legs_with_deadtime(dev, modes, duty_permille);
    return FD6288Q_OK;
}

fd6288q_status_t fd6288q_set_step(fd6288q_t *dev,
                                  uint8_t step,
                                  uint16_t duty_permille)
{
    if (dev == NULL || !dev->initialized)
    {
        return FD6288Q_ERROR_NOT_INIT;
    }
    if (step >= FD6288Q_STEP_N)
    {
        return FD6288Q_ERROR_PARAM;
    }

    duty_permille = clamp_duty(duty_permille);
    apply_legs_with_deadtime(dev, s_step_tbl[step], duty_permille);
    dev->step = step;
    dev->duty_permille = duty_permille;
    return FD6288Q_OK;
}

fd6288q_status_t fd6288q_set_duty(fd6288q_t *dev, uint16_t duty_permille)
{
    uint8_t i;

    if (dev == NULL || !dev->initialized)
    {
        return FD6288Q_ERROR_NOT_INIT;
    }

    duty_permille = clamp_duty(duty_permille);
    for (i = 0u; i < FD6288Q_PHASE_N; i++)
    {
        if (dev->leg_mode[i] == FD6288Q_LEG_PWM)
        {
            dev->leg_apply(i, FD6288Q_LEG_PWM, duty_permille);
        }
    }
    dev->duty_permille = duty_permille;
    return FD6288Q_OK;
}

fd6288q_status_t fd6288q_emergency_stop(fd6288q_t *dev)
{
    if (dev == NULL || !dev->initialized)
    {
        return FD6288Q_ERROR_NOT_INIT;
    }

    apply_all_float(dev);
    apply_enable_pin(dev, false);
    dev->enabled = false;
    dev->duty_permille = 0u;
    return FD6288Q_OK;
}

bool fd6288q_is_initialized(const fd6288q_t *dev)
{
    return (dev != NULL) && dev->initialized;
}

bool fd6288q_is_enabled(const fd6288q_t *dev)
{
    return (dev != NULL) && dev->initialized && dev->enabled;
}

uint8_t fd6288q_get_step(const fd6288q_t *dev)
{
    return (dev != NULL && dev->initialized) ? dev->step : 0u;
}

uint16_t fd6288q_get_duty(const fd6288q_t *dev)
{
    return (dev != NULL && dev->initialized) ? dev->duty_permille : 0u;
}

uint32_t fd6288q_get_deadtime_ns(const fd6288q_t *dev)
{
    return (dev != NULL && dev->initialized) ? dev->deadtime_ns : 0u;
}

fd6288q_status_t fd6288q_fault_get(const fd6288q_t *dev, bool *faulted)
{
    int raw;

    if (dev == NULL || !dev->initialized)
    {
        return FD6288Q_ERROR_NOT_INIT;
    }
    if (faulted == NULL)
    {
        return FD6288Q_ERROR_PARAM;
    }

    if (dev->gpio_get == NULL)
    {
        *faulted = false;
        return FD6288Q_OK;
    }

    raw = dev->gpio_get(FD6288Q_PIN_FAULT);
    if (raw < 0)
    {
        *faulted = false;
        return FD6288Q_OK;
    }

    if (dev->fault_active_low)
    {
        *faulted = (raw == 0);
    }
    else
    {
        *faulted = (raw != 0);
    }
    return FD6288Q_OK;
}
