# ESP-IDF aggregated component: one `cbb` target for all chip drivers used by ESP32-S3 projects.
# Each driver lives in its own subdirectory; INCLUDE_DIRS keeps legacy `#include "st7789.h"` paths.

idf_component_register(
    SRCS
        "ds3231/ds3231.c"
        "eeprom/eeprom.c"
        "ili9341/ili9341.c"
        "st7789/st7789.c"
        "st7789/lcd.c"
        "gc9a01/gc9a01.c"
        "mcp4651/mcp4651.c"
        "mpu6050/mpu6050.c"
        "qmi8658a/qmi8658a.c"
        "oled/oled.c"
        "tb6612/tb6612.c"
        "fd6288q/fd6288q.c"
        "hc_sr04/hc_sr04.c"
        "ws2812b/ws2812b.c"
        "ws2812b/ws2812b_esp32.c"
        "xpt2046/xpt2046.c"
        "es8311/es8311.c"
        "ov2640/ov2640.c"
    INCLUDE_DIRS
        "ds3231"
        "eeprom"
        "ili9341"
        "st7789"
        "gc9a01"
        "mcp4651"
        "mpu6050"
        "qmi8658a"
        "oled"
        "tb6612"
        "fd6288q"
        "hc_sr04"
        "ws2812b"
        "xpt2046"
        "es8311"
        "ov2640"
    REQUIRES esp_driver_gpio esp_driver_rmt esp_driver_cam esp_driver_i2c esp_cam_sensor
)
