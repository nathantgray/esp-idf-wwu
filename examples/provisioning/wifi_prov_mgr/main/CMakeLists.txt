idf_component_register(SRCS "app_main.c"
                       PRIV_REQUIRES esp_wifi nvs_flash wifi_provisioning esp_driver_gpio
                       INCLUDE_DIRS ".")
