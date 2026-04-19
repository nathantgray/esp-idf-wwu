/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Temperature Display - NeoPixel 7-Segment Display Driver
 * Controls a 3x 7-segment NeoPixel display with temperature-based color coding
 * Displays temperature values in Fahrenheit with dynamic color transitions
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

/* Temperature thresholds for color transitions (in Fahrenheit) */
#define TEMP_COLD_MIN 32.0f       /* Coldest temperature - freezing (pure green) */
#define TEMP_COOL 50.0f           /* Cool threshold */
#define TEMP_WARM 70.0f           /* Warm threshold - comfortable room temp */
#define TEMP_HOT 85.0f            /* Hot threshold */
#define TEMP_HOT_MAX 100.0f       /* Hottest temperature (pure red) */

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
 * The color changes based on temperature (Fahrenheit):
 * - Below 32°F: Pure green (freezing)
 * - 32°F to 50°F: Green to cyan (cold)
 * - 50°F to 70°F: Cyan to yellow (cool to comfortable)
 * - 70°F to 85°F: Yellow to orange (warm)
 * - 85°F to 100°F: Orange to red (hot)
 * - Above 100°F: Pure red (very hot)
 *
 * @param temperature Temperature value in Fahrenheit (-99.9 to 99.9°F)
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

/**
 * @brief Display provisioning mode indicator (blinking blue)
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t neopixel_display_provisioning(void);

/**
 * @brief Display provisioning success pattern (green then clear)
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t neopixel_display_provisioning_success(void);

#ifdef __cplusplus
}
#endif
