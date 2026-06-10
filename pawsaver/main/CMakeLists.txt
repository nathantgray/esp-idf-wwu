idf_component_register(SRCS "app_main.c"
                            "pawsaver_mqtt.c"
                            "gy906.c"
                       PRIV_REQUIRES esp_wifi nvs_flash wifi_provisioning esp_driver_gpio esp_driver_i2c esp_adc json mdns
                            INCLUDE_DIRS ".")