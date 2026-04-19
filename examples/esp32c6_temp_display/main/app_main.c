/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * ESP32-C6 Temperature Display with WiFi Provisioning and MQTT
 *
 * This application displays temperature information received from Home Assistant
 * on a 3-digit 7-segment NeoPixel display, converted to Fahrenheit. The display 
 * color changes based on temperature: green for cold (32°F), yellow for 
 * comfortable (70°F), and red for hot (100°F).
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
#include <driver/gpio.h>

#include "neopixel_display.h"
#include "mqtt_temp.h"

static const char *TAG = "app_main";

/* Signal Wi-Fi events on this event-group */
const int WIFI_CONNECTED_EVENT = BIT0;
const int WIFI_PROVISIONING_EVENT = BIT1;
static EventGroupHandle_t wifi_event_group = NULL;

/* Button configuration */
#define REPROV_BUTTON_GPIO GPIO_NUM_1

#define PROV_QR_VERSION         "v1"
#define PROV_TRANSPORT_SOFTAP   "softap"
#define PROV_TRANSPORT_BLE      "ble"
#define QRCODE_BASE_URL         "https://espressif.github.io/esp-jumpstart/qrcode.html"

/* ============================================================================
 * Test Pattern Function
 * ============================================================================ */

/**
 * @brief Convert Celsius to Fahrenheit
 */
static float celsius_to_fahrenheit(float celsius) {
    return (celsius * 9.0f / 5.0f) + 32.0f;
}

/**
 * @brief Startup test pattern: count 0-100 on display (in Fahrenheit)
 */
static void startup_test_pattern(void) {
    ESP_LOGI(TAG, "Running startup test pattern: 32-100°F");
    
    for (int i = 32; i <= 100; i++) {
        /* Display the number as temperature in Fahrenheit */
        neopixel_display_temperature((float)i);
        ESP_LOGI(TAG, "Test pattern: %d°F", i);
        
        /* Wait 2 seconds before next number */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "Test pattern complete, display cleared");
    neopixel_display_clear();
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get device service name from MAC address
 */
static void get_device_service_name(char *service_name, size_t max)
{
    uint8_t eth_mac[6];
    const char *ssid_prefix = "PROV_";
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    snprintf(service_name, max, "%s%02X%02X%02X",
             ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
}

static void wifi_init_sta(void)
{
    /* Start Wi-Fi in station mode */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/* ============================================================================
 * Event Handlers
 * ============================================================================ */

/**
 * @brief WiFi and Provisioning event handler
 */
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
            case WIFI_PROV_START:
                ESP_LOGI(TAG, "Provisioning started");
                neopixel_display_provisioning();
                break;
            case WIFI_PROV_CRED_RECV: {
                wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
                ESP_LOGI(TAG, "Received Wi-Fi credentials"
                         "\n\tSSID     : %s\n\tPassword : %s",
                         (const char *) wifi_sta_cfg->ssid,
                         (const char *) wifi_sta_cfg->password);
                break;
            }
            case WIFI_PROV_CRED_FAIL: {
                wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
                ESP_LOGE(TAG, "Provisioning failed!\n\tReason : %s"
                         "\n\tPlease reset to factory and retry provisioning",
                         (*reason == WIFI_PROV_STA_AUTH_ERROR) ?
                         "Wi-Fi station authentication failed" : "Wi-Fi access-point not found");
#ifdef CONFIG_EXAMPLE_RESET_PROV_MGR_ON_FAILURE
                wifi_prov_mgr_reset_sm_state_on_failure();
#endif
                break;
            }
            case WIFI_PROV_CRED_SUCCESS:
                ESP_LOGI(TAG, "Provisioning successful");
                neopixel_display_provisioning_success();
                break;
            case WIFI_PROV_END:
                /* De-initialize manager once provisioning is finished */
                ESP_LOGI(TAG, "Provisioning ended");
                xEventGroupClearBits(wifi_event_group, WIFI_PROVISIONING_EVENT);
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
                ESP_LOGI(TAG, "Disconnected. Connecting to the AP again...");
                esp_wifi_connect();
                break;
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_SOFTAP
            case WIFI_EVENT_AP_STACONNECTED:
                ESP_LOGI(TAG, "SoftAP transport: Connected!");
                break;
            case WIFI_EVENT_AP_STADISCONNECTED:
                ESP_LOGI(TAG, "SoftAP transport: Disconnected!");
                break;
#endif
            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Connected with IP Address:" IPSTR, IP2STR(&event->ip_info.ip));
        /* Signal main application to continue execution */
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_BLE
    } else if (event_base == PROTOCOMM_TRANSPORT_BLE_EVENT) {
        switch (event_id) {
            case PROTOCOMM_TRANSPORT_BLE_CONNECTED:
                ESP_LOGI(TAG, "BLE transport: Connected!");
                break;
            case PROTOCOMM_TRANSPORT_BLE_DISCONNECTED:
                ESP_LOGI(TAG, "BLE transport: Disconnected!");
                break;
            default:
                break;
        }
#endif
    } else if (event_base == PROTOCOMM_SECURITY_SESSION_EVENT) {
        switch (event_id) {
            case PROTOCOMM_SECURITY_SESSION_SETUP_OK:
                ESP_LOGI(TAG, "Secured session established!");
                break;
            case PROTOCOMM_SECURITY_SESSION_INVALID_SECURITY_PARAMS:
                ESP_LOGE(TAG, "Received invalid security parameters for establishing secure session!");
                break;
            case PROTOCOMM_SECURITY_SESSION_CREDENTIALS_MISMATCH:
                ESP_LOGE(TAG, "Received incorrect PoP for establishing secure session!");
                break;
            default:
                break;
        }
    }
}

/* ============================================================================
 * Reprovisioning Button Handler
 * ============================================================================ */

/**
 * @brief Initialize GPIO for reprovisioning button on GPIO 1
 */
static esp_err_t button_init(void) {
    ESP_LOGI(TAG, "Initializing reprovisioning button on GPIO %d", REPROV_BUTTON_GPIO);
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << REPROV_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    return gpio_config(&io_conf);
}

/**
 * @brief Trigger WiFi reprovisioning by resetting manager and restarting provisioning
 */
static esp_err_t trigger_reprovisioning(void) {
    ESP_LOGI(TAG, "Reprovisioning triggered by button press");
    
    /* Show provisioning indicator on display */
    neopixel_display_provisioning();
    
    /* Set provisioning event flag */
    xEventGroupSetBits(wifi_event_group, WIFI_PROVISIONING_EVENT);
    
    /* Stop WiFi connections */
    ESP_LOGI(TAG, "Stopping WiFi");
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    /* Deinitialize provisioning manager */
    ESP_LOGI(TAG, "Deinitializing provisioning manager");
    wifi_prov_mgr_deinit();
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    /* Reinitialize provisioning manager */
    wifi_prov_mgr_config_t config = {
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_BLE
        .scheme = wifi_prov_scheme_ble,
#else
        .scheme = wifi_prov_scheme_softap,
#endif
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
    };
    
    esp_err_t ret = wifi_prov_mgr_init(config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reinitialize provisioning manager: %s", esp_err_to_name(ret));
        return ret;
    }
    
    

    ESP_LOGI(TAG, "Provisioning manager reinitialized");
    
    /* Re-register event handler after reinitialization */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    
    /* Reset provisioning state to allow re-provisioning */
    wifi_prov_mgr_reset_sm_state_on_failure();
    
    /* Get device service name from MAC address */
    char service_name[12];
    get_device_service_name(service_name, sizeof(service_name));
    
    /* Start reprovisioning */
    wifi_prov_security_t security = WIFI_PROV_SECURITY_1;
    const char *pop = "abcd1234";
    const char *service_key = NULL;
    
    ret = wifi_prov_mgr_start_provisioning(security, (const void *) pop, service_name, service_key);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start reprovisioning: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Reprovisioning started");
    ESP_LOGI(TAG, "Service Name: %s", service_name);
    ESP_LOGI(TAG, "PoP (Proof of Possession): %s", pop);
    ESP_LOGI(TAG, "Scan the QR code with your phone to re-provision the device");
    
    return ESP_OK;
}

/**
 * @brief Task to monitor reprovisioning button (GPIO 1)
 */
static void button_task(void *arg) {
    ESP_LOGI(TAG, "Button task started - monitoring GPIO %d for reprovisioning", REPROV_BUTTON_GPIO);
    
    uint8_t last_state = 1;  /* Button is normally high (pull-up enabled) */
    uint32_t press_duration = 0;
    
    while (1) {
        uint8_t current_state = gpio_get_level(REPROV_BUTTON_GPIO);
        
        if (current_state == 0 && last_state == 1) {
            /* Button pressed (falling edge) */
            ESP_LOGI(TAG, "Button pressed");
            press_duration = 0;
        } else if (current_state == 0 && last_state == 0) {
            /* Button still held down */
            press_duration++;
            
            /* Trigger reprovisioning after button held for ~1 second (10 * 100ms) */
            if (press_duration == 10) {
                ESP_LOGI(TAG, "Button held for 1 second - initiating reprovisioning");
                //trigger_reprovisioning();
                ESP_LOGI(TAG, "Button pressed! Resetting provisioning to connect to new WiFi...");
            
                // Disconnect WiFi and reset provisioning
                esp_wifi_disconnect();
                wifi_prov_mgr_reset_provisioning();
            
                ESP_LOGI(TAG, "Restarting device for new WiFi provisioning...");
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                esp_restart();
                    
            }
        } else if (current_state == 1 && last_state == 0) {
            /* Button released (rising edge) */
            ESP_LOGI(TAG, "Button released after %lu iterations", press_duration);
            press_duration = 0;
        }
        
        last_state = current_state;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ============================================================================
 * WiFi Provisioning Setup
 * ============================================================================ */

/**
 * @brief Initialize WiFi provisioning manager
 */
static esp_err_t wifi_prov_setup(void) {
    ESP_LOGI(TAG, "Starting WiFi provisioning setup");

    /* Initialize the event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_event_group = xEventGroupCreate();

    /* Register our event handler for Wi-Fi, IP and Provisioning related events */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    /* Initialize Wi-Fi including netif with default config */
    esp_netif_create_default_wifi_sta();
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_SOFTAP
    esp_netif_create_default_wifi_ap();
#endif /* CONFIG_EXAMPLE_PROV_TRANSPORT_SOFTAP */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Configuration for the provisioning manager - select scheme based on config */
    wifi_prov_mgr_config_t config = {
#ifdef CONFIG_EXAMPLE_PROV_TRANSPORT_BLE
        .scheme = wifi_prov_scheme_ble,
#else
        .scheme = wifi_prov_scheme_softap,
#endif
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
    };

    /* Initialize provisioning manager with the configuration parameters set above */
    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

    bool provisioned = false;
    /* Let's find out if the device is provisioned */
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    /* If device is not yet provisioned start provisioning service */
    if (!provisioned) {
        ESP_LOGI(TAG, "Device not provisioned, starting provisioning");

        /* Get device service name from MAC address */
        char service_name[12];
        get_device_service_name(service_name, sizeof(service_name));

        /* Use Security 1 with proof of possession */
        wifi_prov_security_t security = WIFI_PROV_SECURITY_1;
        const char *pop = "abcd1234";
        const char *service_key = NULL;

        /* Start provisioning service */
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, (const void *) pop, service_name, service_key));

        /* Print provisioning info */
        ESP_LOGI(TAG, "Provisioning started");
        ESP_LOGI(TAG, "Service Name: %s", service_name);
        ESP_LOGI(TAG, "PoP (Proof of Possession): %s", pop);
        ESP_LOGI(TAG, "Scan the QR code with your phone to provision the device");
    } else {
        ESP_LOGI(TAG, "Device already provisioned, connecting to WiFi");
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
        wifi_init_sta();
    }

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

    /* Show startup pattern on display */
    neopixel_display_clear();
    vTaskDelay(pdMS_TO_TICKS(500));

    /* Wait for WiFi connection with longer timeout */
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT,
                                            pdFALSE, pdTRUE, pdMS_TO_TICKS(60000));
    if (!(bits & WIFI_CONNECTED_EVENT)) {
        ESP_LOGW(TAG, "WiFi not connected after 60 seconds, proceeding anyway");
    }

    /* Initialize MQTT */
    if (mqtt_temp_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize MQTT");
        /* Still continue and show display even without MQTT */
    }

    /* Wait for MQTT connection */
    if (!mqtt_temp_wait_connected(10000)) {
        ESP_LOGW(TAG, "MQTT not connected after 10 seconds, will try in loop");
    }

    /* Main display loop */
    while (1) {
        float temp_celsius = mqtt_temp_get_value();

        if (!isnan(temp_celsius)) {
            /* Convert to Fahrenheit for display */
            float temp_fahrenheit = celsius_to_fahrenheit(temp_celsius);
            
            /* Display temperature */
            ESP_LOGI(TAG, "Displaying temperature: %.1f°F (%.1f°C)", temp_fahrenheit, temp_celsius);
            neopixel_display_temperature(temp_fahrenheit);
            mqtt_temp_clear_new_data_flag();
        } else {
            /* No valid temperature yet - show idle pattern */
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

    /* Initialize NVS partition */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* NVS partition was truncated and needs to be erased */
        ESP_LOGI(TAG, "NVS partition is invalid, erasing and reinitializing");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Initialize TCP/IP */
    ESP_ERROR_CHECK(esp_netif_init());

    /* Initialize the NeoPixel display */
    ESP_LOGI(TAG, "Initializing NeoPixel display on GPIO %d", CONFIG_NEOPIXEL_GPIO);
    if (neopixel_display_init(CONFIG_NEOPIXEL_GPIO) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NeoPixel display!");
        return;
    }

    /* Show initial status: clear display */
    neopixel_display_clear();

    /* Run startup test pattern */
    startup_test_pattern();

    /* Initialize button for reprovisioning */
    if (button_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize reprovisioning button");
        return;
    }

    /* Create button monitoring task */
    if (xTaskCreate(button_task, "button_task", 2048, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create button task");
        return;
    }

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
