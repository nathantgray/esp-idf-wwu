/*
 * SPDX-FileCopyrightText: 2024 PawSaver Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * PawSaver Sensor Module - MLX90614 IR Thermal Sensor
 */

#include "pawsaver.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "mlx90614.h"

static const char *TAG = "pawsaver_sensor";


/* I2C and sensor handles */
static i2c_master_bus_handle_t i2c_bus = NULL;
static mlx90614_handle_t mlx_handle = NULL;

esp_err_t pawsaver_sensor_init(void)
{
    esp_err_t ret;

    /* I2C master bus configuration for XIAO ESP32-C6 */
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = CONFIG_PAWSAVER_I2C_SCL_PIN,
        .sda_io_num = CONFIG_PAWSAVER_I2C_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ret = i2c_new_master_bus(&bus_config, &i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Initialize MLX90614 sensor */
    mlx90614_config_t mlx_config = {
        .i2c_addr = MLX90614_I2C_ADDR_DEFAULT,
    };

    ret = mlx90614_init(i2c_bus, &mlx_config, &mlx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize MLX90614: %s", esp_err_to_name(ret));
        i2c_del_master_bus(i2c_bus);
        i2c_bus = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "MLX90614 sensor initialized (SDA=%d, SCL=%d)",
             CONFIG_PAWSAVER_I2C_SDA_PIN, CONFIG_PAWSAVER_I2C_SCL_PIN);
    return ESP_OK;
}

esp_err_t pawsaver_sensor_read(pawsaver_data_t *data)
{
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!mlx_handle) {
        ESP_LOGE(TAG, "Sensor not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret;

    /* Read ambient temperature */
    ret = mlx90614_read_ambient(mlx_handle, &data->ambient_temp);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ambient temperature: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Read object (ground) temperature */
    ret = mlx90614_read_object(mlx_handle, &data->object_temp);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read object temperature: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Read battery voltage and determine mode */
    data->battery_voltage = pawsaver_battery_read();
    data->mode = pawsaver_get_mode(data->battery_voltage);
    data->timestamp = esp_timer_get_time() / 1000; /* Convert µs to ms */

    ESP_LOGI(TAG, "Sensor data: Ambient=%.2f°C, Ground=%.2f°C, Battery=%.2fV, Mode=%d",
             data->ambient_temp, data->object_temp, data->battery_voltage, data->mode);

    return ESP_OK;
}

void pawsaver_sensor_deinit(void)
{
    if (mlx_handle) {
        mlx90614_deinit(mlx_handle);
        mlx_handle = NULL;
    }

    if (i2c_bus) {
        i2c_del_master_bus(i2c_bus);
        i2c_bus = NULL;
    }

    ESP_LOGI(TAG, "Sensor deinitialized");
}
