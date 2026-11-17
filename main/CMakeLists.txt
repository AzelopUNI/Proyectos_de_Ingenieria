idf_component_register(
    SRCS "main.c" "wifi_scan.c" "sensor_data.c" "rf_scan.c" "gps_scan.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event nvs_flash esp_netif esp_timer
             esp_driver_spi esp_driver_gpio esp_driver_uart esp_rom
)
