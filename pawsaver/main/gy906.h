#ifndef GY906_H
#define GY906_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Default I2C address for MLX90614 */
#define MLX90614_I2C_ADDR       0x5A

/* I2C GPIO pins for ESP32-C6 */
#define GY906_SDA_GPIO          22
#define GY906_SCL_GPIO          23

/* MLX90614 RAM addresses */
#define MLX90614_TA             0x06  // Ambient temperature
#define MLX90614_TOBJ1          0x07  // Object temperature

/**
 * @brief Initialize the GY-906 sensor
 * 
 * @return ESP_OK on success
 */
esp_err_t gy906_init(void);

/**
 * @brief Deinitialize the GY-906 sensor
 * 
 * @return ESP_OK on success
 */
esp_err_t gy906_deinit(void);

/**
 * @brief Read ambient temperature
 * 
 * @param temp_c Pointer to store temperature in Celsius
 * @return ESP_OK on success
 */
esp_err_t gy906_read_ambient(float *temp_c);

/**
 * @brief Read object temperature
 * 
 * @param temp_c Pointer to store temperature in Celsius
 * @return ESP_OK on success
 */
esp_err_t gy906_read_object(float *temp_c);

/**
 * @brief Read both ambient and object temperatures
 * 
 * @param ambient_c Pointer to store ambient temperature in Celsius
 * @param object_c Pointer to store object temperature in Celsius
 * @return ESP_OK on success
 */
esp_err_t gy906_read_both(float *ambient_c, float *object_c);

#ifdef __cplusplus
}
#endif

#endif // GY906_H
