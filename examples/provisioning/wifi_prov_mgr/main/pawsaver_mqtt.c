/*
 * SPDX-FileCopyrightText: 2024 PawSaver Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * PawSaver MQTT Module
 */

#include "pawsaver.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "pawsaver_mqtt";

/* MQTT client handle */
static esp_mqtt_client_handle_t mqtt_client = NULL;

/* Event group for connection status */
static EventGroupHandle_t mqtt_event_group = NULL;
#define MQTT_CONNECTED_BIT BIT0

/**
 * @brief MQTT event handler
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected to broker");
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

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT message published, msg_id=%d", event->msg_id);
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

esp_err_t pawsaver_mqtt_init(void)
{
    /* Create event group */
    mqtt_event_group = xEventGroupCreate();
    if (!mqtt_event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }

    /* MQTT client configuration */
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_PAWSAVER_MQTT_BROKER_URI,
        .credentials.username = CONFIG_PAWSAVER_MQTT_USERNAME,
        .credentials.authentication.password = CONFIG_PAWSAVER_MQTT_PASSWORD,
        .session.keepalive = 30,
        .network.timeout_ms = 10000,
    };

    ESP_LOGI(TAG, "Connecting to MQTT broker: %s", CONFIG_PAWSAVER_MQTT_BROKER_URI);

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

bool pawsaver_mqtt_wait_connected(uint32_t timeout_ms)
{
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

esp_err_t pawsaver_mqtt_publish(const pawsaver_data_t *data)
{
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!mqtt_client) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /* Check connection status */
    EventBits_t bits = xEventGroupGetBits(mqtt_event_group);
    if (!(bits & MQTT_CONNECTED_BIT)) {
        ESP_LOGW(TAG, "MQTT not connected, cannot publish");
        return ESP_ERR_INVALID_STATE;
    }

    /* Build JSON payload */
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddNumberToObject(root, "timestamp", (double)data->timestamp);
    cJSON_AddNumberToObject(root, "ambient", (double)data->ambient_temp);
    cJSON_AddNumberToObject(root, "object", (double)data->object_temp);
    cJSON_AddNumberToObject(root, "battery", (double)data->battery_voltage);
    cJSON_AddNumberToObject(root, "mode", data->mode);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!payload) {
        ESP_LOGE(TAG, "Failed to serialize JSON");
        return ESP_ERR_NO_MEM;
    }

    /* Publish message */
    int msg_id = esp_mqtt_client_publish(mqtt_client,
                                          CONFIG_PAWSAVER_MQTT_TOPIC,
                                          payload,
                                          0,    /* Length (0 = use strlen) */
                                          1,    /* QoS 1 */
                                          0);   /* Retain = false */

    ESP_LOGI(TAG, "Published to '%s': %s (msg_id=%d)",
             CONFIG_PAWSAVER_MQTT_TOPIC, payload, msg_id);

    free(payload);

    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

void pawsaver_mqtt_deinit(void)
{
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

esp_err_t publish_device_discovery(void)
{
    if (!mqtt_client) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /* Check connection status */
    EventBits_t bits = xEventGroupGetBits(mqtt_event_group);
    if (!(bits & MQTT_CONNECTED_BIT)) {
        ESP_LOGW(TAG, "MQTT not connected, cannot publish");
        return ESP_ERR_INVALID_STATE;
    }

    /* Create common device information (shared by all sensors) */
    cJSON *device = cJSON_CreateObject();
    if (!device) {
        ESP_LOGE(TAG, "Failed to create device object");
        return ESP_ERR_NO_MEM;
    }
    
    cJSON_AddStringToObject(device, "name", "PawSaver");
    cJSON_AddStringToObject(device, "manufacturer", "PawSaver Project");
    cJSON_AddStringToObject(device, "model", "ESP32 TempMonitor");
    cJSON_AddStringToObject(device, "sw_version", "1.0.0");
    
    cJSON *identifiers = cJSON_CreateArray();
    cJSON_AddItemToArray(identifiers, cJSON_CreateString("pawsaver_esp32"));
    cJSON_AddItemToObject(device, "identifiers", identifiers);

    /* Publish Ambient Temperature Sensor Discovery */
    cJSON *ambient_config = cJSON_CreateObject();
    if (ambient_config) {
        cJSON_AddStringToObject(ambient_config, "name", "Ambient Temperature");
        cJSON_AddStringToObject(ambient_config, "unique_id", "pawsaver_ambient_temp");
        cJSON_AddStringToObject(ambient_config, "state_topic", CONFIG_PAWSAVER_MQTT_TOPIC);
        cJSON_AddStringToObject(ambient_config, "unit_of_measurement", "°C");
        cJSON_AddStringToObject(ambient_config, "value_template", "{{ value_json.ambient }}");
        cJSON_AddStringToObject(ambient_config, "device_class", "temperature");
        cJSON_AddItemToObject(ambient_config, "device", cJSON_Duplicate(device, 1));
        
        char *ambient_payload = cJSON_PrintUnformatted(ambient_config);
        cJSON_Delete(ambient_config);
        
        if (ambient_payload) {
            esp_mqtt_client_publish(mqtt_client,
                                    "homeassistant/sensor/pawsaver/ambient_temperature/config",
                                    ambient_payload, 0, 1, 1);
            ESP_LOGI(TAG, "Published ambient temp discovery");
            free(ambient_payload);
        }
    }

    /* Publish Object Temperature Sensor Discovery */
    cJSON *object_config = cJSON_CreateObject();
    if (object_config) {
        cJSON_AddStringToObject(object_config, "name", "Object Temperature");
        cJSON_AddStringToObject(object_config, "unique_id", "pawsaver_object_temp");
        cJSON_AddStringToObject(object_config, "state_topic", CONFIG_PAWSAVER_MQTT_TOPIC);
        cJSON_AddStringToObject(object_config, "unit_of_measurement", "°C");
        cJSON_AddStringToObject(object_config, "value_template", "{{ value_json.object }}");
        cJSON_AddStringToObject(object_config, "device_class", "temperature");
        cJSON_AddItemToObject(object_config, "device", cJSON_Duplicate(device, 1));
        
        char *object_payload = cJSON_PrintUnformatted(object_config);
        cJSON_Delete(object_config);
        
        if (object_payload) {
            esp_mqtt_client_publish(mqtt_client,
                                    "homeassistant/sensor/pawsaver/object_temperature/config",
                                    object_payload, 0, 1, 1);
            ESP_LOGI(TAG, "Published object temp discovery");
            free(object_payload);
        }
    }

    /* Publish Battery Voltage Sensor Discovery */
    cJSON *battery_config = cJSON_CreateObject();
    if (battery_config) {
        cJSON_AddStringToObject(battery_config, "name", "Battery Voltage");
        cJSON_AddStringToObject(battery_config, "unique_id", "pawsaver_battery");
        cJSON_AddStringToObject(battery_config, "state_topic", CONFIG_PAWSAVER_MQTT_TOPIC);
        cJSON_AddStringToObject(battery_config, "unit_of_measurement", "V");
        cJSON_AddStringToObject(battery_config, "value_template", "{{ value_json.battery }}");
        cJSON_AddStringToObject(battery_config, "device_class", "voltage");
        cJSON_AddItemToObject(battery_config, "device", cJSON_Duplicate(device, 1));
        
        char *battery_payload = cJSON_PrintUnformatted(battery_config);
        cJSON_Delete(battery_config);
        
        if (battery_payload) {
            esp_mqtt_client_publish(mqtt_client,
                                    "homeassistant/sensor/pawsaver/battery_voltage/config",
                                    battery_payload, 0, 1, 1);
            ESP_LOGI(TAG, "Published battery discovery");
            free(battery_payload);
        }
    }

    cJSON_Delete(device);
    
    ESP_LOGI(TAG, "All discovery messages published");
    return ESP_OK;
}
