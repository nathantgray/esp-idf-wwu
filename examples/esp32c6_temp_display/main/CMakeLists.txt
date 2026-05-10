idf_component_register(SRCS "app_main.c"
                            "neopixel_display.c"
                            "mqtt_temp.c"
                       PRIV_REQUIRES esp_wifi nvs_flash wifi_provisioning bt esp_driver_gpio esp_driver_rmt json mqtt neopixel qrcode
                       INCLUDE_DIRS ".")
