/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Temperature Display - MQTT Temperature Subscriber
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize MQTT client for temperature subscriptions
 *
 * Configures and starts the MQTT client to subscribe to Home Assistant
 * temperature entities.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t mqtt_temp_init(void);

/**
 * @brief Deinitialize MQTT client
 */
void mqtt_temp_deinit(void);

/**
 * @brief Check if MQTT is connected to broker
 *
 * @return true if connected, false otherwise
 */
bool mqtt_temp_is_connected(void);

/**
 * @brief Wait for MQTT connection with timeout
 *
 * @param timeout_ms Maximum time to wait in milliseconds
 * @return true if connected, false if timed out
 */
bool mqtt_temp_wait_connected(uint32_t timeout_ms);

/**
 * @brief Get the current temperature value from last MQTT message
 *
 * @return Temperature in Celsius, or NaN if no valid reading
 */
float mqtt_temp_get_value(void);

/**
 * @brief Check if a new temperature reading is available
 *
 * @return true if new data available, false otherwise
 */
bool mqtt_temp_has_new_data(void);

/**
 * @brief Clear the new data flag
 */
void mqtt_temp_clear_new_data_flag(void);

#ifdef __cplusplus
}
#endif
