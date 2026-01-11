/*
 * SPDX-FileCopyrightText: 2024 PawSaver Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * PawSaver Battery Monitor Module
 */

#include "pawsaver.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "pawsaver_battery";

/* ADC handles */
static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t cali_handle = NULL;
static bool cali_available = false;

/* Voltage divider ratio from Kconfig (stored as x1000) */
#define VOLTAGE_DIVIDER_RATIO   (CONFIG_PAWSAVER_VOLTAGE_DIVIDER_RATIO / 1000.0f)

esp_err_t pawsaver_battery_init(void)
{
    esp_err_t ret;

    /* ADC unit configuration */
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ret = adc_oneshot_new_unit(&unit_cfg, &adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init ADC unit: %s", esp_err_to_name(ret));
        return ret;
    }

    /* ADC channel configuration */
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_oneshot_config_channel(adc_handle, CONFIG_PAWSAVER_BATTERY_ADC_CHANNEL, &chan_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config ADC channel: %s", esp_err_to_name(ret));
        adc_oneshot_del_unit(adc_handle);
        adc_handle = NULL;
        return ret;
    }

    /* ADC calibration - try curve fitting first, then line fitting */
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali_handle);
    if (ret == ESP_OK) {
        cali_available = true;
        ESP_LOGI(TAG, "ADC calibration: curve fitting");
    }
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_cali_create_scheme_line_fitting(&cali_cfg, &cali_handle);
    if (ret == ESP_OK) {
        cali_available = true;
        ESP_LOGI(TAG, "ADC calibration: line fitting");
    }
#endif

    if (!cali_available) {
        ESP_LOGW(TAG, "ADC calibration not available, using raw values");
    }

    ESP_LOGI(TAG, "Battery ADC initialized (channel=%d, divider=%.3f)",
             CONFIG_PAWSAVER_BATTERY_ADC_CHANNEL, VOLTAGE_DIVIDER_RATIO);
    return ESP_OK;
}

float pawsaver_battery_read(void)
{
    if (!adc_handle) {
        ESP_LOGE(TAG, "Battery ADC not initialized");
        return 0.0f;
    }

    int raw_value = 0;
    int voltage_mv = 0;

    /* Read raw ADC value */
    esp_err_t ret = adc_oneshot_read(adc_handle, CONFIG_PAWSAVER_BATTERY_ADC_CHANNEL, &raw_value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ADC: %s", esp_err_to_name(ret));
        return 0.0f;
    }

    /* Convert to voltage */
    if (cali_available && cali_handle) {
        adc_cali_raw_to_voltage(cali_handle, raw_value, &voltage_mv);
    } else {
        /* Fallback: assume 3.3V reference, 12-bit ADC */
        voltage_mv = (raw_value * 3300) / 4095;
    }

    /* Apply voltage divider ratio to get actual battery voltage */
    float battery_voltage = (voltage_mv / 1000.0f) * VOLTAGE_DIVIDER_RATIO;

    ESP_LOGD(TAG, "Battery: raw=%d, adc_mv=%d, voltage=%.2fV", 
             raw_value, voltage_mv, battery_voltage);

    return battery_voltage;
}

pawsaver_mode_t pawsaver_get_mode(float battery_voltage)
{
    if (battery_voltage >= BATTERY_PLUGGED_IN) {
        ESP_LOGI(TAG, "Mode: DEBUG (plugged in)");
        return PAWSAVER_MODE_DEBUG;
    } else if (battery_voltage < BATTERY_MIN_VOLTAGE) {
        ESP_LOGW(TAG, "Mode: DEAD (critical battery)");
        return PAWSAVER_MODE_DEAD;
    } else if (battery_voltage < BATTERY_LOW_VOLTAGE) {
        ESP_LOGI(TAG, "Mode: LOW_POWER");
        return PAWSAVER_MODE_LOW_POWER;
    }
    
    ESP_LOGI(TAG, "Mode: NORMAL");
    return PAWSAVER_MODE_NORMAL;
}

uint32_t pawsaver_get_sleep_duration(pawsaver_mode_t mode)
{
    switch (mode) {
        case PAWSAVER_MODE_DEBUG:
            return TIME_TO_SLEEP_DEBUG;
        case PAWSAVER_MODE_LOW_POWER:
            return TIME_TO_SLEEP_LONG;
        case PAWSAVER_MODE_DEAD:
            return TIME_TO_SLEEP_DEAD;
        case PAWSAVER_MODE_NORMAL:
        default:
            return TIME_TO_SLEEP_SHORT;
    }
}

void pawsaver_battery_deinit(void)
{
    if (cali_handle) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        adc_cali_delete_scheme_curve_fitting(cali_handle);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        adc_cali_delete_scheme_line_fitting(cali_handle);
#endif
        cali_handle = NULL;
        cali_available = false;
    }

    if (adc_handle) {
        adc_oneshot_del_unit(adc_handle);
        adc_handle = NULL;
    }

    ESP_LOGI(TAG, "Battery ADC deinitialized");
}
