/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * ESP32-C6 Temperature Display with WiFi Provisioning and MQTT
 *
 * This application displays temperature information received from Home Assistant
 * on a 3-digit 7-segment NeoPixel display. The display color changes based on
 * temperature: green for cold, red for hot.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_softap.h>

#include "neopixel_display.h"
#include "mqtt_temp.h"

static const char *TAG = "app_main";

/* WiFi event group */
const int WIFI_CONNECTED_EVENT = BIT0;
static EventGroupHandle_t wifi_event_group = NULL;

#define PROV_QR_VERSION "v1"
#define PROV_TRANSPORT_SOFTAP "softap"
#define PROV_TRANSPORT_BLE "ble"

/* ============================================================================
 * Event Handlers
 * ============================================================================ */

/**
 * @brief WiFi and Provisioning event handler
 */
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
            case WIFI_PROV_START:
                ESP_LOGI(TAG, "Provisioning started");
                break;
                
            case WIFI_PROV_CRED_RECV: {
                wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
                ESP_LOGI(TAG, "Received Wi-Fi credentials");
                ESP_LOGI(TAG, "  SSID: %s", (const char *)wifi_sta_cfg->ssid);
                break;
            }
            
            case WIFI_PROV_CRED_FAIL: {
                wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
                ESP_LOGE(TAG, "Provisioning failed!");
                ESP_LOGE(TAG, "  Reason: %s",
                         (*reason == WIFI_PROV_STA_AUTH_ERROR) ?
                         "Wi-Fi authentication failed" : "Wi-Fi access point not found");
                wifi_prov_mgr_reset_sm_state_on_failure();
                break;
            }
            
            case WIFI_PROV_CRED_SUCCESS:
                ESP_LOGI(TAG, "Provisioning successful");
                break;
                
            case WIFI_PROV_END:
                ESP_LOGI(TAG, "Provisioning finished, de-initializing");
                wifi_prov_mgr_deinit();
                break;
                
            default:
                break;
        }
    } else if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi started, attempting connection...");
                esp_wifi_connect();
                break;
                
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGW(TAG, "WiFi disconnected, reconnecting...");
                esp_wifi_connect();
                break;
                
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_SOFTAP
            case WIFI_EVENT_AP_STACONNECTED:
                ESP_LOGI(TAG, "SoftAP: Station connected");
                break;
            case WIFI_EVENT_AP_STADISCONNECTED:
                ESP_LOGI(TAG, "SoftAP: Station disconnected");
                break;
#endif
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "WiFi connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        
        if (wifi_event_group) {
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);
        }
    }
}

/* ============================================================================
 * WiFi Provisioning Setup
 * ============================================================================ */

/**
 * @brief Initialize WiFi provisioning
 */
static esp_err_t wifi_prov_setup(void) {
    ESP_LOGI(TAG, "Starting WiFi provisioning setup");

    /* Initialize/get NVS partition */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(TAG, "NVS needs erasing, formatting...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Create WiFi event group */
    wifi_event_group = xEventGroupCreate();
    if (!wifi_event_group) {
        ESP_LOGE(TAG, "Failed to create WiFi event group");
        return ESP_ERR_NO_MEM;
    }

    /* Initialize TCP/IP stack and WiFi */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    /* WiFi initialization */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Register event handlers */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID,
                                                &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                &event_handler, NULL));

    /* Check if we're already provisioned */
    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    if (!provisioned) {
        ESP_LOGI(TAG, "Device not provisioned, starting provisioning");

        /* Configure provisioning manager */
        wifi_prov_mgr_config_t config = {
            .scheme = wifi_prov_scheme_softap,
            .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE,
            .app_event_handler = WIFI_PROV_EVENT_HANDLER_NONE,
            .wifi_prov_conn_cfg = {.wifi_conn_attempts = 0},
        };

        ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

        /* Get provisioning data */
        const char *service_name = CONFIG_PROV_SERVICE_NAME;
        const char *pop = CONFIG_PROV_POP;

        /* SoftAP SSID details */
        char ssid[32];
        snprintf(ssid, sizeof(ssid), "%s_%02x%02x%02x",
                 service_name,
                 0, 0, 0);  /* MAC address would go here */

        ESP_LOGI(TAG, "SoftAP SSID: %s", ssid);
        ESP_LOGI(TAG, "Pop value: %s", pop);

        /* Start provisioning service */
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(
            WIFI_PROV_SECURITY_1, pop, ssid, NULL));
    } else {
        ESP_LOGI(TAG, "Device already provisioned, connecting to WiFi");
    }

    /* Start WiFi */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

/* ============================================================================
 * Display Update Task
 * ============================================================================ */

/**
 * @brief Task that updates the display with temperature data
 */
static void display_update_task(void *arg) {
    ESP_LOGI(TAG, "Display update task started");

    /* Wait for WiFi connection */
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT,
                                            pdFALSE, pdTRUE, pdMS_TO_TICKS(30000));
    if (!(bits & WIFI_CONNECTED_EVENT)) {
        ESP_LOGW(TAG, "WiFi not connected after 30 seconds, proceeding anyway");
    }

    /* Initialize MQTT */
    if (mqtt_temp_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize MQTT");
        vTaskDelete(NULL);
        return;
    }

    /* Wait for MQTT connection */
    if (!mqtt_temp_wait_connected(10000)) {
        ESP_LOGW(TAG, "MQTT not connected after 10 seconds");
    }

    /* Main display loop */
    while (1) {
        float temp = mqtt_temp_get_value();

        if (!isnan(temp)) {
            /* Display temperature */
            ESP_LOGI(TAG, "Displaying temperature: %.1f°C", temp);
            neopixel_display_temperature(temp);
            mqtt_temp_clear_new_data_flag();
        } else {
            /* No valid temperature yet - show 0 in blue indicator */
            ESP_LOGD(TAG, "Waiting for temperature data...");
            neopixel_display_clear();
        }

        /* Update every 100ms */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ============================================================================
 * Main Application Entry Point
 * ============================================================================ */

void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ESP32-C6 Temperature Display Starting");
    ESP_LOGI(TAG, "========================================");

    /* Initialize the NeoPixel display */
    ESP_LOGI(TAG, "Initializing NeoPixel display on GPIO %d", CONFIG_NEOPIXEL_GPIO);
    if (neopixel_display_init(CONFIG_NEOPIXEL_GPIO) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NeoPixel display!");
        return;
    }

    /* Show initial status: clear display */
    neopixel_display_clear();

    /* Setup WiFi and Provisioning */
    if (wifi_prov_setup() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to setup WiFi provisioning!");
        return;
    }

    /* Create display update task */
    if (xTaskCreate(display_update_task, "display_update", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create display update task");
        return;
    }

    ESP_LOGI(TAG, "Application initialized successfully");
}
