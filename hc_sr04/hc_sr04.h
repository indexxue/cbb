/**
 * @file    hc_sr04.h
 * @brief   HC-SR04 / HC-SR04P ultrasonic rangefinder (GPIO callback injection, MCU-agnostic).
 */

#ifndef HC_SR04_H
#define HC_SR04_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Logical pin IDs mapped by the board adapter. */
#define HC_SR04_PIN_TRIG  0u
#define HC_SR04_PIN_ECHO  1u

/** Default trigger pulse width (us). Datasheet minimum is 10 us. */
#define HC_SR04_TRIG_PULSE_US_DEFAULT   10u

/** Wait for echo edges; ~30 ms covers the HC-SR04 maximum range (~4 m). */
#define HC_SR04_ECHO_TIMEOUT_US_DEFAULT 30000u

/** Speed of sound at ~20 °C, cm/s (used for distance conversion). */
#define HC_SR04_SPEED_CM_PER_S_DEFAULT  34300u

/** Datasheet valid range (mm). */
#define HC_SR04_RANGE_MIN_MM_DEFAULT    20u
#define HC_SR04_RANGE_MAX_MM_DEFAULT    4000u

typedef void (*hc_sr04_gpio_set_t)(uint8_t pin_id, uint8_t level);
typedef uint8_t (*hc_sr04_gpio_read_t)(uint8_t pin_id);
typedef void (*hc_sr04_delay_us_t)(uint32_t us);
typedef uint32_t (*hc_sr04_get_tick_us_t)(void);

typedef enum {
    HC_SR04_OK = 0,
    HC_SR04_ERROR_PARAM,
    HC_SR04_ERROR_NOT_INIT,
    HC_SR04_ERROR_TIMEOUT,
    HC_SR04_ERROR_OUT_OF_RANGE,
} hc_sr04_status_t;

typedef struct {
    hc_sr04_gpio_set_t    gpio_set;
    hc_sr04_gpio_read_t   gpio_read;
    hc_sr04_delay_us_t    delay_us;
    hc_sr04_get_tick_us_t get_tick_us;
    uint16_t              trig_pulse_us;
    uint32_t              echo_timeout_us;
    uint32_t              speed_cm_per_s;
    uint16_t              range_min_mm;
    uint16_t              range_max_mm;
} hc_sr04_config_t;

typedef struct {
    hc_sr04_gpio_set_t    gpio_set;
    hc_sr04_gpio_read_t   gpio_read;
    hc_sr04_delay_us_t    delay_us;
    hc_sr04_get_tick_us_t get_tick_us;
    uint16_t              trig_pulse_us;
    uint32_t              echo_timeout_us;
    uint32_t              speed_cm_per_s;
    uint16_t              range_min_mm;
    uint16_t              range_max_mm;
    bool                  initialized;
} hc_sr04_t;

hc_sr04_status_t hc_sr04_init_with_config(hc_sr04_t *dev, const hc_sr04_config_t *cfg);

hc_sr04_status_t hc_sr04_init(hc_sr04_t *dev,
                              hc_sr04_gpio_set_t gpio_set,
                              hc_sr04_gpio_read_t gpio_read,
                              hc_sr04_delay_us_t delay_us,
                              hc_sr04_get_tick_us_t get_tick_us);

bool hc_sr04_is_initialized(const hc_sr04_t *dev);

/**
 * @brief Measure round-trip echo time and convert to distance (mm).
 * @param distance_mm Output distance in millimeters on success.
 */
hc_sr04_status_t hc_sr04_measure_mm(hc_sr04_t *dev, uint16_t *distance_mm);

/**
 * @brief Measure distance in centimeters (integer truncation of mm result).
 */
hc_sr04_status_t hc_sr04_measure_cm(hc_sr04_t *dev, uint8_t *distance_cm);

/**
 * @brief Convert echo pulse width (us) to distance (mm).
 *        distance_mm = pulse_us * speed_cm_per_s / 20000
 */
uint16_t hc_sr04_pulse_us_to_mm(uint32_t pulse_us, uint32_t speed_cm_per_s);

#ifdef __cplusplus
}
#endif

#endif /* HC_SR04_H */
