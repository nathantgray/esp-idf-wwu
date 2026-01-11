/*
 * SPDX-FileCopyrightText: 2024 PawSaver Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * PawSaver Deep Sleep Module
 */

#include "pawsaver.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "pawsaver_sleep";


/* Microseconds per second */
#define uS_TO_S_FACTOR 1000000ULL

const char* pawsaver_get_wakeup_reason_str(void)
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

    switch (cause) {
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            return "Power on / reset";
        case ESP_SLEEP_WAKEUP_EXT0:
            return "External signal (RTC_IO)";
        case ESP_SLEEP_WAKEUP_EXT1:
            return "External signal (RTC_CNTL)";
        case ESP_SLEEP_WAKEUP_TIMER:
            return "Timer";
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            return "Touchpad";
        case ESP_SLEEP_WAKEUP_ULP:
            return "ULP program";
        case ESP_SLEEP_WAKEUP_GPIO:
            return "GPIO";
        case ESP_SLEEP_WAKEUP_UART:
            return "UART";
        default:
            return "Unknown";
    }
}

void pawsaver_enter_deep_sleep(pawsaver_mode_t mode)
{
    uint32_t sleep_duration = pawsaver_get_sleep_duration(mode);
    uint64_t sleep_time_us = (uint64_t)sleep_duration * uS_TO_S_FACTOR;

    ESP_LOGI(TAG, "Entering deep sleep for %lu seconds (mode=%d)", 
             (unsigned long)sleep_duration, mode);
    ESP_LOGI(TAG, "Wakeup will occur at approximately T+%lu sec", 
             (unsigned long)sleep_duration);

    /* Configure timer wakeup */
    esp_err_t ret = esp_sleep_enable_timer_wakeup(sleep_time_us);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable timer wakeup: %s", esp_err_to_name(ret));
        /* Fall back to a short sleep */
        esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP_DEBUG * uS_TO_S_FACTOR);
    }

    /* Give time for log messages to flush */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Enter deep sleep - this function does not return */
    esp_deep_sleep_start();

    /* Should never reach here */
    ESP_LOGE(TAG, "Deep sleep failed!");
}
