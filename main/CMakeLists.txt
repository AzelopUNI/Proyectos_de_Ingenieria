idf_component_register(
    SRCS "main.c" "wifi_scan.c" "sensor_data.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event nvs_flash esp_netif esp_timer
)
