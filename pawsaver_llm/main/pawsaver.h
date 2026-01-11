/*
 * SPDX-FileCopyrightText: 2024 PawSaver Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * PawSaver - Ground Temperature Monitor for Dog Walking Safety
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif


/* ============================================================================
 * Power Mode Definitions
 * ============================================================================ */

typedef enum {
    PAWSAVER_MODE_NORMAL = 0,    /**< Normal operation mode */
    PAWSAVER_MODE_LOW_POWER = 1, /**< Low power mode - reduced reporting frequency */
    PAWSAVER_MODE_DEAD = 2,      /**< Critical battery - minimal operation */
    PAWSAVER_MODE_DEBUG = 3      /**< Debug mode - frequent updates when plugged in */
} pawsaver_mode_t;

/* Sleep durations in seconds */
#define TIME_TO_SLEEP_DEBUG     5       /**< Debug mode: 5 second sleep */
#define TIME_TO_SLEEP_SHORT     60      /**< Normal mode: 60 second sleep */
#define TIME_TO_SLEEP_LONG      300     /**< Low power mode: 5 minute sleep */
#define TIME_TO_SLEEP_DEAD      3600    /**< Dead mode: 1 hour sleep */

/* Battery voltage thresholds (in Volts) */
#define BATTERY_MIN_VOLTAGE     3.73f   /**< Below this = DEAD mode */
#define BATTERY_LOW_VOLTAGE     3.84f   /**< Below this = LOW_POWER mode */
#define BATTERY_FULL_VOLTAGE    4.02f   /**< Fully charged battery */
#define BATTERY_PLUGGED_IN      4.4f    /**< Above this = plugged in (DEBUG mode) */

/* ============================================================================
 * Sensor Data Structure
 * ============================================================================ */

typedef struct {
    float ambient_temp;         /**< Ambient temperature in Celsius */
    float object_temp;          /**< Ground/object temperature in Celsius */
    float battery_voltage;      /**< Battery voltage in Volts */
    pawsaver_mode_t mode;       /**< Current power mode */
    int64_t timestamp;          /**< Timestamp in milliseconds since boot */
} pawsaver_data_t;

/* ============================================================================
 * Sensor Functions (pawsaver_sensor.c)
 * ============================================================================ */

/**
 * @brief Initialize the MLX90614 IR thermal sensor
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t pawsaver_sensor_init(void);

/**
 * @brief Read all sensor data (temperature and battery)
 *
 * @param data Pointer to structure to fill with sensor data
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t pawsaver_sensor_read(pawsaver_data_t *data);

/**
 * @brief Deinitialize sensor (for power saving before sleep)
 */
void pawsaver_sensor_deinit(void);

/* ============================================================================
 * Battery Functions (pawsaver_battery.c)
 * ============================================================================ */

/**
 * @brief Initialize battery voltage ADC
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t pawsaver_battery_init(void);

/**
 * @brief Read current battery voltage
 *
 * @return Battery voltage in Volts
 */
float pawsaver_battery_read(void);

/**
 * @brief Determine power mode based on battery voltage
 *
 * @param battery_voltage Current battery voltage
 * @return Appropriate power mode
 */
pawsaver_mode_t pawsaver_get_mode(float battery_voltage);

/**
 * @brief Get sleep duration for a given power mode
 *
 * @param mode Power mode
 * @return Sleep duration in seconds
 */
uint32_t pawsaver_get_sleep_duration(pawsaver_mode_t mode);

/**
 * @brief Deinitialize battery ADC (for power saving before sleep)
 */
void pawsaver_battery_deinit(void);

/* ============================================================================
 * MQTT Functions (pawsaver_mqtt.c)
 * ============================================================================ */

/**
 * @brief Initialize MQTT client and connect to broker
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t pawsaver_mqtt_init(void);

/**
 * @brief Wait for MQTT connection to be established
 *
 * @param timeout_ms Timeout in milliseconds
 * @return true if connected, false on timeout
 */
bool pawsaver_mqtt_wait_connected(uint32_t timeout_ms);

/**
 * @brief Publish sensor data to MQTT broker
 *
 * @param data Pointer to sensor data to publish
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t pawsaver_mqtt_publish(const pawsaver_data_t *data);

/**
 * @brief Stop and cleanup MQTT client
 */
void pawsaver_mqtt_deinit(void);

/* ============================================================================
 * Sleep Functions (pawsaver_sleep.c)
 * ============================================================================ */

/**
 * @brief Enter deep sleep with timer wakeup
 *
 * @param mode Power mode (determines sleep duration)
 */
void pawsaver_enter_deep_sleep(pawsaver_mode_t mode);

/**
 * @brief Get the wakeup cause after reset
 *
 * @return String describing wakeup cause
 */
const char* pawsaver_get_wakeup_reason_str(void);

#ifdef __cplusplus
}
#endif
