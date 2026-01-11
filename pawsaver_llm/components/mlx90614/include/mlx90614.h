/*
 * SPDX-FileCopyrightText: 2024 PawSaver Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * MLX90614 IR Thermometer Driver for ESP-IDF
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Default I2C address for MLX90614 */
#define MLX90614_I2C_ADDR_DEFAULT   0x5A

/* MLX90614 RAM addresses */
#define MLX90614_REG_AMBIENT_TEMP   0x06    /**< Ambient temperature */
#define MLX90614_REG_OBJECT1_TEMP   0x07    /**< Object 1 temperature */
#define MLX90614_REG_OBJECT2_TEMP   0x08    /**< Object 2 temperature (dual zone only) */

/* MLX90614 EEPROM addresses (add 0x20 for read) */
#define MLX90614_REG_EMISSIVITY     0x24    /**< Emissivity coefficient */
#define MLX90614_REG_CONFIG         0x25    /**< Config register */

/* Temperature conversion constant */
#define MLX90614_TEMP_SCALE         0.02f   /**< LSB = 0.02°C */
#define MLX90614_TEMP_OFFSET        273.15f /**< Kelvin to Celsius offset */

/**
 * @brief MLX90614 device handle
 */
typedef struct mlx90614_dev_t *mlx90614_handle_t;

/**
 * @brief MLX90614 configuration
 */
typedef struct {
    uint8_t i2c_addr;   /**< I2C device address (default: 0x5A) */
} mlx90614_config_t;

/**
 * @brief Initialize MLX90614 sensor
 *
 * @param bus_handle I2C master bus handle
 * @param config Sensor configuration
 * @param handle_out Pointer to store device handle
 * @return ESP_OK on success
 */
esp_err_t mlx90614_init(i2c_master_bus_handle_t bus_handle, 
                        const mlx90614_config_t *config,
                        mlx90614_handle_t *handle_out);

/**
 * @brief Read ambient temperature
 *
 * @param handle Device handle
 * @param temp_c Pointer to store temperature in Celsius
 * @return ESP_OK on success
 */
esp_err_t mlx90614_read_ambient(mlx90614_handle_t handle, float *temp_c);

/**
 * @brief Read object (IR) temperature
 *
 * @param handle Device handle
 * @param temp_c Pointer to store temperature in Celsius
 * @return ESP_OK on success
 */
esp_err_t mlx90614_read_object(mlx90614_handle_t handle, float *temp_c);

/**
 * @brief Read both ambient and object temperatures
 *
 * @param handle Device handle
 * @param ambient_c Pointer to store ambient temperature in Celsius
 * @param object_c Pointer to store object temperature in Celsius
 * @return ESP_OK on success
 */
esp_err_t mlx90614_read_both(mlx90614_handle_t handle, float *ambient_c, float *object_c);

/**
 * @brief Set emissivity coefficient
 *
 * @param handle Device handle
 * @param emissivity Emissivity value (0.1 to 1.0)
 * @return ESP_OK on success
 */
esp_err_t mlx90614_set_emissivity(mlx90614_handle_t handle, float emissivity);

/**
 * @brief Get current emissivity coefficient
 *
 * @param handle Device handle
 * @param emissivity Pointer to store emissivity value
 * @return ESP_OK on success
 */
esp_err_t mlx90614_get_emissivity(mlx90614_handle_t handle, float *emissivity);

/**
 * @brief Deinitialize MLX90614 sensor
 *
 * @param handle Device handle
 */
void mlx90614_deinit(mlx90614_handle_t handle);

#ifdef __cplusplus
}
#endif
