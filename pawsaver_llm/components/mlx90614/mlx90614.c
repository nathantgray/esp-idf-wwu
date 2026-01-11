/*
 * SPDX-FileCopyrightText: 2024 PawSaver Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * MLX90614 IR Thermometer Driver Implementation
 */

#include <stdlib.h>
#include <string.h>
#include "mlx90614.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "mlx90614";

/* I2C timing */
#define MLX90614_I2C_TIMEOUT_MS     100
#define MLX90614_I2C_SPEED_HZ       100000  /* 100kHz for SMBus */

/**
 * @brief MLX90614 device structure
 */
struct mlx90614_dev_t {
    i2c_master_dev_handle_t i2c_dev;    /**< I2C device handle */
    uint8_t addr;                        /**< Device address */
};

/**
 * @brief Read 16-bit value from MLX90614 register
 */
static esp_err_t mlx90614_read_reg16(mlx90614_handle_t handle, uint8_t reg, uint16_t *value)
{
    if (!handle || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data[3];  /* 2 bytes data + 1 byte PEC (we ignore PEC) */
    
    esp_err_t ret = i2c_master_transmit_receive(handle->i2c_dev, 
                                                 &reg, 1, 
                                                 data, 3,
                                                 MLX90614_I2C_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read register 0x%02X: %s", reg, esp_err_to_name(ret));
        return ret;
    }

    /* MLX90614 returns LSB first */
    *value = (uint16_t)(data[0] | (data[1] << 8));
    
    return ESP_OK;
}

/**
 * @brief Convert raw temperature to Celsius
 */
static float mlx90614_raw_to_celsius(uint16_t raw)
{
    /* Temperature = raw * 0.02 - 273.15 */
    return (raw * MLX90614_TEMP_SCALE) - MLX90614_TEMP_OFFSET;
}

esp_err_t mlx90614_init(i2c_master_bus_handle_t bus_handle,
                        const mlx90614_config_t *config,
                        mlx90614_handle_t *handle_out)
{
    if (!bus_handle || !config || !handle_out) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Allocate device structure */
    struct mlx90614_dev_t *dev = calloc(1, sizeof(struct mlx90614_dev_t));
    if (!dev) {
        ESP_LOGE(TAG, "Failed to allocate device structure");
        return ESP_ERR_NO_MEM;
    }

    dev->addr = config->i2c_addr ? config->i2c_addr : MLX90614_I2C_ADDR_DEFAULT;

    /* Configure I2C device */
    i2c_device_config_t i2c_dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev->addr,
        .scl_speed_hz = MLX90614_I2C_SPEED_HZ,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &i2c_dev_cfg, &dev->i2c_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device: %s", esp_err_to_name(ret));
        free(dev);
        return ret;
    }

    /* Verify communication by reading ambient temperature */
    uint16_t test_val;
    ret = mlx90614_read_reg16(dev, MLX90614_REG_AMBIENT_TEMP, &test_val);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to communicate with MLX90614 at address 0x%02X", dev->addr);
        i2c_master_bus_rm_device(dev->i2c_dev);
        free(dev);
        return ret;
    }

    ESP_LOGI(TAG, "MLX90614 initialized at address 0x%02X", dev->addr);
    *handle_out = dev;

    return ESP_OK;
}

esp_err_t mlx90614_read_ambient(mlx90614_handle_t handle, float *temp_c)
{
    if (!handle || !temp_c) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t raw;
    esp_err_t ret = mlx90614_read_reg16(handle, MLX90614_REG_AMBIENT_TEMP, &raw);
    if (ret != ESP_OK) {
        return ret;
    }

    /* Check for error flag (bit 15) */
    if (raw & 0x8000) {
        ESP_LOGE(TAG, "Sensor error flag set");
        return ESP_ERR_INVALID_RESPONSE;
    }

    *temp_c = mlx90614_raw_to_celsius(raw);
    ESP_LOGD(TAG, "Ambient: raw=0x%04X, temp=%.2f°C", raw, *temp_c);

    return ESP_OK;
}

esp_err_t mlx90614_read_object(mlx90614_handle_t handle, float *temp_c)
{
    if (!handle || !temp_c) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t raw;
    esp_err_t ret = mlx90614_read_reg16(handle, MLX90614_REG_OBJECT1_TEMP, &raw);
    if (ret != ESP_OK) {
        return ret;
    }

    /* Check for error flag (bit 15) */
    if (raw & 0x8000) {
        ESP_LOGE(TAG, "Sensor error flag set");
        return ESP_ERR_INVALID_RESPONSE;
    }

    *temp_c = mlx90614_raw_to_celsius(raw);
    ESP_LOGD(TAG, "Object: raw=0x%04X, temp=%.2f°C", raw, *temp_c);

    return ESP_OK;
}

esp_err_t mlx90614_read_both(mlx90614_handle_t handle, float *ambient_c, float *object_c)
{
    esp_err_t ret;

    ret = mlx90614_read_ambient(handle, ambient_c);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = mlx90614_read_object(handle, object_c);
    if (ret != ESP_OK) {
        return ret;
    }

    return ESP_OK;
}

esp_err_t mlx90614_get_emissivity(mlx90614_handle_t handle, float *emissivity)
{
    if (!handle || !emissivity) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t raw;
    esp_err_t ret = mlx90614_read_reg16(handle, MLX90614_REG_EMISSIVITY, &raw);
    if (ret != ESP_OK) {
        return ret;
    }

    /* Emissivity = raw / 65535 */
    *emissivity = raw / 65535.0f;
    ESP_LOGD(TAG, "Emissivity: raw=0x%04X, value=%.3f", raw, *emissivity);

    return ESP_OK;
}

esp_err_t mlx90614_set_emissivity(mlx90614_handle_t handle, float emissivity)
{
    /* Setting emissivity requires EEPROM write - not implemented yet */
    /* This requires erasing EEPROM cell first, then writing new value */
    ESP_LOGW(TAG, "set_emissivity not implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

void mlx90614_deinit(mlx90614_handle_t handle)
{
    if (!handle) {
        return;
    }

    if (handle->i2c_dev) {
        i2c_master_bus_rm_device(handle->i2c_dev);
    }

    free(handle);
    ESP_LOGI(TAG, "MLX90614 deinitialized");
}
