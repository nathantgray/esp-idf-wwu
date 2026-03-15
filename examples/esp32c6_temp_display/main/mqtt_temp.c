/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Temperature Display - MQTT Temperature Subscriber Implementation
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "esp_log.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mqtt_temp.h"

static const char *TAG = "mqtt_temp";

/* MQTT client handle */
static esp_mqtt_client_handle_t mqtt_client = NULL;

/* Event group for connection status */
static EventGroupHandle_t mqtt_event_group = NULL;
#define MQTT_CONNECTED_BIT BIT0
#define MQTT_NEW_DATA_BIT BIT1

/* Current temperature value and lock */
static float current_temperature = NAN;
static portMUX_TYPE temp_lock = portMUX_INITIALIZER_UNLOCKED;

/**
 * @brief Parse temperature from MQTT message payload
 *
 * Handles both plain numeric values and JSON payloads with different formats
 */
static float parse_temperature(const char *payload, int payload_len) {
    char payload_str[payload_len + 1];
    memcpy(payload_str, payload, payload_len);
    payload_str[payload_len] = '\0';

    /* Try to parse as JSON first */
    cJSON *json = cJSON_Parse(payload_str);
    if (json != NULL) {
        /* Look for common temperature field names */
        cJSON *temp_value = cJSON_GetObjectItem(json, "value");
        if (temp_value == NULL) {
            temp_value = cJSON_GetObjectItem(json, "temperature");
        }
        if (temp_value == NULL) {
            temp_value = cJSON_GetObjectItem(json, "temp");
        }
        if (temp_value == NULL) {
            temp_value = cJSON_GetObjectItem(json, "state");
        }

        if (temp_value != NULL && temp_value->type == cJSON_Number) {
            float result = (float)temp_value->valuedouble;
            cJSON_Delete(json);
            return result;
        }
        cJSON_Delete(json);
    }

    /* Try to parse as plain number */
    char *endptr;
    float value = strtof(payload_str, &endptr);
    if (endptr != payload_str) {
        return value;
    }

    ESP_LOGW(TAG, "Could not parse temperature from payload: %s", payload_str);
    return NAN;
}

/**
 * @brief MQTT event handler
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected to broker");
            
            /* Subscribe to the temperature topic */
            const char *sub_topic = CONFIG_MQTT_TEMP_TOPIC;
            ESP_LOGI(TAG, "Subscribing to topic: %s", sub_topic);
            esp_mqtt_client_subscribe(mqtt_client, sub_topic, 0);
            
            if (mqtt_event_group) {
                xEventGroupSetBits(mqtt_event_group, MQTT_CONNECTED_BIT);
            }
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected");
            if (mqtt_event_group) {
                xEventGroupClearBits(mqtt_event_group, MQTT_CONNECTED_BIT);
            }
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT subscribed, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGD(TAG, "MQTT data received");
            ESP_LOGD(TAG, "Topic: %.*s", event->topic_len, event->topic);
            ESP_LOGD(TAG, "Data: %.*s", event->data_len, event->data);
            
            /* Parse temperature from payload */
            float temp = parse_temperature(event->data, event->data_len);
            if (!isnan(temp)) {
                portENTER_CRITICAL(&temp_lock);
                current_temperature = temp;
                portEXIT_CRITICAL(&temp_lock);
                
                ESP_LOGI(TAG, "Temperature updated: %.1f°C", temp);
                
                /* Signal that new data is available */
                if (mqtt_event_group) {
                    xEventGroupSetBits(mqtt_event_group, MQTT_NEW_DATA_BIT);
                }
            }
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "  Transport error: %s", 
                         esp_err_to_name(event->error_handle->esp_transport_sock_errno));
            }
            break;

        default:
            ESP_LOGD(TAG, "MQTT event: %ld", (long)event_id);
            break;
    }
}

esp_err_t mqtt_temp_init(void) {
    /* Create event group */
    mqtt_event_group = xEventGroupCreate();
    if (!mqtt_event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Initializing MQTT client");
    ESP_LOGI(TAG, "  Broker: %s", CONFIG_MQTT_BROKER_URI);
    ESP_LOGI(TAG, "  Username: %s", CONFIG_MQTT_USERNAME);
    ESP_LOGI(TAG, "  Topic: %s", CONFIG_MQTT_TEMP_TOPIC);

    /* MQTT client configuration */
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_MQTT_BROKER_URI,
        .credentials.username = CONFIG_MQTT_USERNAME,
        .credentials.authentication.password = CONFIG_MQTT_PASSWORD,
        .session.keepalive = 30,
        .network.timeout_ms = 10000,
    };

    /* Initialize client */
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!mqtt_client) {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        vEventGroupDelete(mqtt_event_group);
        mqtt_event_group = NULL;
        return ESP_FAIL;
    }

    /* Register event handler */
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    /* Start client */
    esp_err_t ret = esp_mqtt_client_start(mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(ret));
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        vEventGroupDelete(mqtt_event_group);
        mqtt_event_group = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "MQTT client started");
    return ESP_OK;
}

void mqtt_temp_deinit(void) {
    if (mqtt_client) {
        esp_mqtt_client_stop(mqtt_client);
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
    }

    if (mqtt_event_group) {
        vEventGroupDelete(mqtt_event_group);
        mqtt_event_group = NULL;
    }

    ESP_LOGI(TAG, "MQTT client stopped");
}

bool mqtt_temp_is_connected(void) {
    if (!mqtt_event_group) {
        return false;
    }

    EventBits_t bits = xEventGroupGetBits(mqtt_event_group);
    return (bits & MQTT_CONNECTED_BIT) != 0;
}

bool mqtt_temp_wait_connected(uint32_t timeout_ms) {
    if (!mqtt_event_group) {
        return false;
    }

    EventBits_t bits = xEventGroupWaitBits(mqtt_event_group,
                                            MQTT_CONNECTED_BIT,
                                            pdFALSE,  /* Don't clear on exit */
                                            pdTRUE,   /* Wait for all bits */
                                            pdMS_TO_TICKS(timeout_ms));

    return (bits & MQTT_CONNECTED_BIT) != 0;
}

float mqtt_temp_get_value(void) {
    portENTER_CRITICAL(&temp_lock);
    float value = current_temperature;
    portEXIT_CRITICAL(&temp_lock);
    return value;
}

bool mqtt_temp_has_new_data(void) {
    if (!mqtt_event_group) {
        return false;
    }

    EventBits_t bits = xEventGroupGetBits(mqtt_event_group);
    return (bits & MQTT_NEW_DATA_BIT) != 0;
}

void mqtt_temp_clear_new_data_flag(void) {
    if (mqtt_event_group) {
        xEventGroupClearBits(mqtt_event_group, MQTT_NEW_DATA_BIT);
    }
}
