#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"

void wifi_scan(void)
{
    uint16_t ap_count = 0;

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true
    };

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));

    wifi_ap_record_t ap_records[50];

    if(ap_count > 50)
        ap_count = 50;

    ESP_ERROR_CHECK(
        esp_wifi_scan_get_ap_records(&ap_count, ap_records)
    );

    printf("\nFound %u APs\n\n", ap_count);

    for(int i=0; i<ap_count; i++)
    {
        printf(
            "%2d | CH:%2d | RSSI:%4d | %s\n",
            i,
            ap_records[i].primary,
            ap_records[i].rssi,
            (char*)ap_records[i].ssid
        );
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(
        esp_wifi_set_mode(WIFI_MODE_STA)
    );

    ESP_ERROR_CHECK(esp_wifi_start());

    while(1)
    {
        wifi_scan();

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
