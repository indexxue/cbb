/**
 * @file    hc_sr04.c
 * @brief   HC-SR04 / HC-SR04P ultrasonic rangefinder (GPIO callback injection).
 */

#include "hc_sr04.h"

#include <stddef.h>

static hc_sr04_status_t hc_sr04_check_ready(const hc_sr04_t *dev)
{
    if (dev == NULL) {
        return HC_SR04_ERROR_PARAM;
    }
    if (!dev->initialized || (dev->gpio_set == NULL) || (dev->gpio_read == NULL) ||
        (dev->delay_us == NULL) || (dev->get_tick_us == NULL)) {
        return HC_SR04_ERROR_NOT_INIT;
    }
    return HC_SR04_OK;
}

static void hc_sr04_apply_defaults(hc_sr04_config_t *cfg)
{
    if (cfg->trig_pulse_us == 0u) {
        cfg->trig_pulse_us = HC_SR04_TRIG_PULSE_US_DEFAULT;
    }
    if (cfg->echo_timeout_us == 0u) {
        cfg->echo_timeout_us = HC_SR04_ECHO_TIMEOUT_US_DEFAULT;
    }
    if (cfg->speed_cm_per_s == 0u) {
        cfg->speed_cm_per_s = HC_SR04_SPEED_CM_PER_S_DEFAULT;
    }
    if (cfg->range_min_mm == 0u) {
        cfg->range_min_mm = HC_SR04_RANGE_MIN_MM_DEFAULT;
    }
    if (cfg->range_max_mm == 0u) {
        cfg->range_max_mm = HC_SR04_RANGE_MAX_MM_DEFAULT;
    }
}

static hc_sr04_status_t hc_sr04_wait_level(hc_sr04_t *dev,
                                           uint8_t pin_id,
                                           uint8_t level,
                                           uint32_t timeout_us,
                                           uint32_t *elapsed_us)
{
    uint32_t start_us = dev->get_tick_us();

    while (dev->gpio_read(pin_id) != level) {
        uint32_t now_us = dev->get_tick_us();
        uint32_t delta_us = now_us - start_us;

        if (delta_us >= timeout_us) {
            if (elapsed_us != NULL) {
                *elapsed_us = delta_us;
            }
            return HC_SR04_ERROR_TIMEOUT;
        }
    }

    if (elapsed_us != NULL) {
        *elapsed_us = dev->get_tick_us() - start_us;
    }
    return HC_SR04_OK;
}

uint16_t hc_sr04_pulse_us_to_mm(uint32_t pulse_us, uint32_t speed_cm_per_s)
{
    uint64_t mm;

    if ((pulse_us == 0u) || (speed_cm_per_s == 0u)) {
        return 0u;
    }

    /* pulse_us 是往返时间(µs)，speed_cm_per_s 是声速(cm/s)
     * 距离(mm) = pulse_us×10⁻⁶ × speed×10 / 2 = pulse_us × speed / 200000 */
    mm = ((uint64_t)pulse_us * (uint64_t)speed_cm_per_s) / 200000u;
    if (mm > 0xFFFFu) {
        return 0xFFFFu;
    }
    return (uint16_t)mm;
}

hc_sr04_status_t hc_sr04_init_with_config(hc_sr04_t *dev, const hc_sr04_config_t *cfg)
{
    hc_sr04_config_t local_cfg;

    if ((dev == NULL) || (cfg == NULL) || (cfg->gpio_set == NULL) || (cfg->gpio_read == NULL) ||
        (cfg->delay_us == NULL) || (cfg->get_tick_us == NULL)) {
        return HC_SR04_ERROR_PARAM;
    }

    local_cfg = *cfg;
    hc_sr04_apply_defaults(&local_cfg);

    dev->gpio_set        = local_cfg.gpio_set;
    dev->gpio_read       = local_cfg.gpio_read;
    dev->delay_us        = local_cfg.delay_us;
    dev->get_tick_us     = local_cfg.get_tick_us;
    dev->trig_pulse_us   = local_cfg.trig_pulse_us;
    dev->echo_timeout_us = local_cfg.echo_timeout_us;
    dev->speed_cm_per_s  = local_cfg.speed_cm_per_s;
    dev->range_min_mm    = local_cfg.range_min_mm;
    dev->range_max_mm    = local_cfg.range_max_mm;
    dev->initialized     = false;

    dev->gpio_set(HC_SR04_PIN_TRIG, 0u);
    dev->delay_us(2u);
    dev->initialized = true;
    return HC_SR04_OK;
}

hc_sr04_status_t hc_sr04_init(hc_sr04_t *dev,
                              hc_sr04_gpio_set_t gpio_set,
                              hc_sr04_gpio_read_t gpio_read,
                              hc_sr04_delay_us_t delay_us,
                              hc_sr04_get_tick_us_t get_tick_us)
{
    hc_sr04_config_t cfg = {0};

    cfg.gpio_set    = gpio_set;
    cfg.gpio_read   = gpio_read;
    cfg.delay_us    = delay_us;
    cfg.get_tick_us = get_tick_us;
    return hc_sr04_init_with_config(dev, &cfg);
}

bool hc_sr04_is_initialized(const hc_sr04_t *dev)
{
    return (dev != NULL) && dev->initialized;
}

hc_sr04_status_t hc_sr04_measure_mm(hc_sr04_t *dev, uint16_t *distance_mm)
{
    hc_sr04_status_t st;
    uint32_t pulse_us = 0u;
    uint16_t mm;

    st = hc_sr04_check_ready(dev);
    if (st != HC_SR04_OK) {
        return st;
    }
    if (distance_mm == NULL) {
        return HC_SR04_ERROR_PARAM;
    }

    dev->gpio_set(HC_SR04_PIN_TRIG, 0u);
    dev->delay_us(2u);
    dev->gpio_set(HC_SR04_PIN_TRIG, 1u);
    dev->delay_us((uint32_t)dev->trig_pulse_us);
    dev->gpio_set(HC_SR04_PIN_TRIG, 0u);

    st = hc_sr04_wait_level(dev, HC_SR04_PIN_ECHO, 1u, dev->echo_timeout_us, NULL);
    if (st != HC_SR04_OK) {
        return st;
    }

    st = hc_sr04_wait_level(dev, HC_SR04_PIN_ECHO, 0u, dev->echo_timeout_us, &pulse_us);
    if (st != HC_SR04_OK) {
        return st;
    }

    mm = hc_sr04_pulse_us_to_mm(pulse_us, dev->speed_cm_per_s);
    if ((mm < dev->range_min_mm) || (mm > dev->range_max_mm)) {
        return HC_SR04_ERROR_OUT_OF_RANGE;
    }

    *distance_mm = mm;
    return HC_SR04_OK;
}

hc_sr04_status_t hc_sr04_measure_cm(hc_sr04_t *dev, uint8_t *distance_cm)
{
    hc_sr04_status_t st;
    uint16_t mm = 0u;

    st = hc_sr04_measure_mm(dev, &mm);
    if (st != HC_SR04_OK) {
        return st;
    }
    if (distance_cm == NULL) {
        return HC_SR04_ERROR_PARAM;
    }

    *distance_cm = (uint8_t)(mm / 10u);
    return HC_SR04_OK;
}
