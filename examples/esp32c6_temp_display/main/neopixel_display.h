/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Temperature Display - NeoPixel 7-Segment Display Driver
 * Controls a 3x 7-segment NeoPixel display with temperature-based color coding
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Display Configuration */
#define NUM_DIGITS 3
#define LEDS_PER_SEGMENT 4
#define SEGMENTS_PER_DIGIT 7
#define LEDS_PER_DIGIT (LEDS_PER_SEGMENT * SEGMENTS_PER_DIGIT)
#define TOTAL_LEDS (NUM_DIGITS * LEDS_PER_DIGIT)

/* Temperature thresholds for color transitions (in Celsius) */
#define TEMP_COLD_MIN -20.0f      /* Coldest temperature (pure green) */
#define TEMP_COOL 5.0f            /* Cool threshold */
#define TEMP_WARM 20.0f           /* Warm threshold */
#define TEMP_HOT 35.0f            /* Hot threshold */
#define TEMP_HOT_MAX 50.0f        /* Hottest temperature (pure red) */

/**
 * @brief Initialize the NeoPixel display
 *
 * @param gpio_pin The GPIO pin connected to the NeoPixel data line
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t neopixel_display_init(int gpio_pin);

/**
 * @brief Deinitialize the NeoPixel display
 */
void neopixel_display_deinit(void);

/**
 * @brief Display a temperature value on the 3-digit 7-segment display
 *
 * The color changes based on temperature:
 * - Below -20°C: Pure green
 * - -20°C to 5°C: Green to cyan
 * - 5°C to 20°C: Cyan to yellow
 * - 20°C to 35°C: Yellow to orange
 * - 35°C to 50°C: Orange to red
 * - Above 50°C: Pure red
 *
 * @param temperature Temperature value in Celsius (-99.9 to 99.9)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t neopixel_display_temperature(float temperature);

/**
 * @brief Get the RGB color for a given temperature
 *
 * @param temperature Temperature in Celsius
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 */
void neopixel_get_color_for_temp(float temperature, uint8_t *r, uint8_t *g, uint8_t *b);

/**
 * @brief Clear the display (turn off all LEDs)
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t neopixel_display_clear(void);

/**
 * @brief Set brightness level (0-255)
 *
 * @param brightness Brightness level (0 = off, 255 = max)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t neopixel_display_set_brightness(uint8_t brightness);

#ifdef __cplusplus
}
#endif
