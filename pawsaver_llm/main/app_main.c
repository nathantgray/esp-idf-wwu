/*
 * SPDX-FileCopyrightText: 2024 PawSaver Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * PawSaver - Ground Temperature Monitor for Dog Walking Safety
 * 
 * Main application: Wi-Fi provisioning via BLE, sensor reading, MQTT publishing
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "wifi_provisioning/manager.h"

#include "wifi_provisioning/scheme_ble.h"
#include "qrcode.h"
#include "pawsaver.h"

static const char *TAG = "pawsaver";

/* Event group for Wi-Fi connection */
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_EVENT BIT0

/**
 * @brief Event handler for Wi-Fi and provisioning events
 */
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
            case WIFI_PROV_START:
                ESP_LOGI(TAG, "Provisioning started");
                break;
            case WIFI_PROV_CRED_RECV: {
                wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
                ESP_LOGI(TAG, "Received Wi-Fi credentials - SSID: %s", (char *)wifi_sta_cfg->ssid);
                break;
            }
            case WIFI_PROV_CRED_FAIL: {
                wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
                ESP_LOGE(TAG, "Provisioning failed: %s",
                         (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "Auth error" : "AP not found");
                wifi_prov_mgr_reset_sm_state_on_failure();
                break;
            }
            case WIFI_PROV_CRED_SUCCESS:
                ESP_LOGI(TAG, "Provisioning successful");
                break;
            case WIFI_PROV_END:
                wifi_prov_mgr_deinit();
                break;
            default:
                break;
        }
    } else if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "Wi-Fi disconnected, reconnecting...");
                esp_wifi_connect();
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);
    }
}

/**
 * @brief Initialize Wi-Fi in station mode
 */
static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/**
 * @brief Get device service name for provisioning
 */
static void get_device_service_name(char *service_name, size_t max_len)
{
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(service_name, max_len, "PROV_PAWSAVER_%02X%02X", mac[4], mac[5]);
}

/**
 * @brief Print QR code for provisioning
 */
static void print_qr_code(const char *service_name)
{
    char payload[150];

    snprintf(payload, sizeof(payload),
             "{\"ver\":\"v1\",\"name\":\"%s\",\"transport\":\"ble\"}",
             service_name);

    ESP_LOGI(TAG, "Scan this QR code from the ESP provisioning app:");
    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
    esp_qrcode_generate(&cfg, payload);
    ESP_LOGI(TAG, "Or use: %s", payload);
}

/**
 * @brief Main application entry point
 */
void app_main(void)
{
    esp_err_t ret;

    /* Print wakeup reason */
    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "PawSaver - Ground Temperature Monitor");
    ESP_LOGI(TAG, "Wakeup cause: %s", pawsaver_get_wakeup_reason_str());
    ESP_LOGI(TAG, "============================================");

    /* Initialize NVS - erase to reset provisioning for testing */
    ESP_LOGW(TAG, "Erasing NVS to reset WiFi provisioning...");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
    ESP_ERROR_CHECK(ret);

    /* Initialize PawSaver hardware */
    ESP_LOGI(TAG, "Initializing hardware...");
    ESP_ERROR_CHECK(pawsaver_battery_init());
    
    /* Try to initialize sensor - continue even if it fails (for testing without hardware) */
    bool sensor_available = false;
    ret = pawsaver_sensor_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "MLX90614 sensor not available - running in debug mode");
        sensor_available = false;
    } else {
        sensor_available = true;
    }

    /* Read sensor data or use dummy data for debug */
    pawsaver_data_t sensor_data = {0};
    if (sensor_available) {
        ret = pawsaver_sensor_read(&sensor_data);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to read sensor, using debug data");
            sensor_available = false;
        }
    }
    
    /* If sensor not available, populate with debug data */
    if (!sensor_available) {
        sensor_data.timestamp = (int64_t)(esp_timer_get_time() / 1000000);
        sensor_data.ambient_temp = -999.0f;  /* Sentinel value indicating no sensor */
        sensor_data.object_temp = -999.0f;
        sensor_data.battery_voltage = pawsaver_battery_read();
        sensor_data.mode = PAWSAVER_MODE_DEBUG;
        ESP_LOGI(TAG, "Using debug data: battery=%.2fV", sensor_data.battery_voltage);
    }

    /* Initialize network stack */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_event_group = xEventGroupCreate();

    /* Register event handlers */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    /* Initialize Wi-Fi */
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Initialize provisioning manager */
    wifi_prov_mgr_config_t prov_config = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(prov_config));

    /* Check if device is already provisioned */
    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    if (!provisioned) {
        ESP_LOGI(TAG, "Device not provisioned, starting provisioning...");

        /* Get device name for provisioning */
        char service_name[32];
        get_device_service_name(service_name, sizeof(service_name));

        /* Configure security (Security Version 1 - simple PoP password) */
        wifi_prov_security_t security = WIFI_PROV_SECURITY_1;
        const char *pop = "abcd1234";  /* Proof of Possession password */
        wifi_prov_security1_params_t *sec_params = (wifi_prov_security1_params_t *)pop;

        /* Start provisioning */
        uint8_t custom_service_uuid[] = {
            0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
            0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02,
        };
        wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid);

        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, sec_params, service_name, NULL));
        print_qr_code(service_name);

        /* Wait for provisioning to complete */
        xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, false, true, portMAX_DELAY);

    } else {
        ESP_LOGI(TAG, "Already provisioned, connecting to AP...");
        wifi_prov_mgr_deinit();
        wifi_init_sta();

        /* Wait for connection with timeout */
        EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT,
                                                false, true, pdMS_TO_TICKS(30000));
        if (!(bits & WIFI_CONNECTED_EVENT)) {
            ESP_LOGE(TAG, "Failed to connect to Wi-Fi, sleeping...");
            pawsaver_enter_deep_sleep(sensor_data.mode);
        }
    }

    ESP_LOGI(TAG, "Connected to Wi-Fi!");

    /* Initialize MQTT */
    ESP_ERROR_CHECK(pawsaver_mqtt_init());

    /* Wait for MQTT connection */
    if (!pawsaver_mqtt_wait_connected(10000)) {
        ESP_LOGE(TAG, "MQTT connection timeout");
    } else {
        /* Publish sensor data */
        pawsaver_mqtt_publish(&sensor_data);

        /* Wait for message to be sent */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    /* Cleanup */
    pawsaver_mqtt_deinit();
    if (sensor_available) {
        pawsaver_sensor_deinit();
    }
    pawsaver_battery_deinit();

    /* Enter deep sleep (skip in debug mode for easier testing) */
    if (sensor_data.mode == PAWSAVER_MODE_DEBUG && !sensor_available) {
        ESP_LOGI(TAG, "Debug mode without sensor - staying awake for 30 seconds...");
        vTaskDelay(pdMS_TO_TICKS(30000));
        ESP_LOGI(TAG, "Restarting...");
        esp_restart();
    } else {
        ESP_LOGI(TAG, "Cycle complete, entering deep sleep...");
        pawsaver_enter_deep_sleep(sensor_data.mode);
    }
}