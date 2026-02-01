#include "gy906.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "GY906";

#define I2C_MASTER_FREQ_HZ      100000
#define I2C_MASTER_TIMEOUT_MS   1000

static bool initialized = false;
static i2c_master_bus_handle_t bus_handle = NULL;
static i2c_master_dev_handle_t dev_handle = NULL;

esp_err_t gy906_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    // Configure I2C master bus
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GY906_SDA_GPIO,
        .scl_io_num = GY906_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_config, &bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Add MLX90614 device to the bus
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MLX90614_I2C_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    ret = i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device: %s", esp_err_to_name(ret));
        i2c_del_master_bus(bus_handle);
        return ret;
    }

    initialized = true;
    ESP_LOGI(TAG, "GY-906 initialized (SDA: GPIO%d, SCL: GPIO%d)", GY906_SDA_GPIO, GY906_SCL_GPIO);
    return ESP_OK;
}

esp_err_t gy906_deinit(void)
{
    if (!initialized) {
        return ESP_OK;
    }

    if (dev_handle) {
        i2c_master_bus_rm_device(dev_handle);
        dev_handle = NULL;
    }

    if (bus_handle) {
        i2c_del_master_bus(bus_handle);
        bus_handle = NULL;
    }

    initialized = false;
    ESP_LOGI(TAG, "GY-906 deinitialized");
    return ESP_OK;
}

/**
 * @brief Read 16-bit data from MLX90614 register
 */
static esp_err_t mlx90614_read_reg(uint8_t reg, uint16_t *data)
{
    if (!initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t buf[3];
    
    // Write register address and read 3 bytes (2 data + 1 PEC)
    esp_err_t ret = i2c_master_transmit_receive(dev_handle, &reg, 1, buf, 3, I2C_MASTER_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read register: %s", esp_err_to_name(ret));
        return ret;
    }

    // Combine low and high bytes
    *data = (buf[1] << 8) | buf[0];
    
    return ESP_OK;
}

/**
 * @brief Convert raw MLX90614 temperature to Celsius
 */
static float mlx90614_raw_to_celsius(uint16_t raw)
{
    // MLX90614 returns temperature in 0.02K units
    return (raw * 0.02f) - 273.15f;
}

esp_err_t gy906_read_ambient(float *temp_c)
{
    if (temp_c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t raw;
    esp_err_t ret = mlx90614_read_reg(MLX90614_TA, &raw);
    if (ret != ESP_OK) {
        return ret;
    }

    *temp_c = mlx90614_raw_to_celsius(raw);
    ESP_LOGD(TAG, "Ambient temperature: %.2f°C", *temp_c);
    
    return ESP_OK;
}

esp_err_t gy906_read_object(float *temp_c)
{
    if (temp_c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t raw;
    esp_err_t ret = mlx90614_read_reg(MLX90614_TOBJ1, &raw);
    if (ret != ESP_OK) {
        return ret;
    }

    *temp_c = mlx90614_raw_to_celsius(raw);
    ESP_LOGD(TAG, "Object temperature: %.2f°C", *temp_c);
    
    return ESP_OK;
}

esp_err_t gy906_read_both(float *ambient_c, float *object_c)
{
    if (ambient_c == NULL || object_c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = gy906_read_ambient(ambient_c);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = gy906_read_object(object_c);
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_LOGI(TAG, "Ambient: %.2f°C, Object: %.2f°C", *ambient_c, *object_c);
    
    return ESP_OK;
}
