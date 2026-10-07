#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "wifi_scan.h"

static const char *TAG = "wifi_scan";

// Numero maximo de APs que leemos de golpe tras el escaneo.
// 20 suele ser mas que suficiente en calle; si haceis pruebas en zonas muy
// densas (ej. universidad) podeis subirlo.
#define WIFI_SCAN_MAX_AP_RECORDS 20

esp_err_t wifi_scan_init(void)
{
    esp_err_t err;

    // NVS es necesario porque el driver WiFi guarda calibracion ahi
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Creamos la interfaz STA aunque no nos vayamos a conectar a ningun AP;
    // es necesaria para poder usar el escaner.
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Guardamos la configuracion WiFi en RAM, no en flash (NVS), para no
    // desgastar la flash con escrituras repetidas durante el proyecto.
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi inicializado en modo STA (solo escaneo, sin conexion)");
    return ESP_OK;
}

esp_err_t wifi_scan_perform(wifi_scan_result_t *result)
{
    if (result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(wifi_scan_result_t));

    int64_t start_us = esp_timer_get_time();

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,              // 0 = recorrer todos los canales
        .show_hidden = true,       // contar tambien APs con SSID oculto
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,  // ms minimos de escucha por canal
        .scan_time.active.max = 300,  // ms maximos de escucha por canal
    };

    // 'true' = bloqueante, el propio esp_wifi_scan_start espera a que termine
    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al iniciar el escaneo WiFi: %s", esp_err_to_name(err));
        return err;
    }

    uint16_t ap_num = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_num));

    if (ap_num > WIFI_SCAN_MAX_AP_RECORDS) {
        ap_num = WIFI_SCAN_MAX_AP_RECORDS;
    }

    wifi_ap_record_t ap_records[WIFI_SCAN_MAX_AP_RECORDS];
    uint16_t ap_count = ap_num;
    err = esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al leer los registros de APs: %s", esp_err_to_name(err));
        return err;
    }

    // --- Procesado / agregacion de los datos crudos ---
    // En vez de guardar la lista completa de APs (SSID, BSSID, etc. -no
    // necesario y mas pesado de publicar-), calculamos metricas agregadas.
    int32_t rssi_sum = 0;
    int8_t rssi_max = -127; // valor muy bajo de partida

    for (int i = 0; i < ap_count; i++) {
        int8_t rssi = ap_records[i].rssi;
        uint8_t channel = ap_records[i].primary;

        rssi_sum += rssi;
        if (rssi > rssi_max) {
            rssi_max = rssi;
        }

        if (channel >= 1 && channel < WIFI_SCAN_NUM_CHANNELS) {
            result->ap_count_per_channel[channel]++;
        }
    }

    result->ap_count = (uint8_t)ap_count;
    result->rssi_max = (ap_count > 0) ? rssi_max : 0;
    result->rssi_avg = (ap_count > 0) ? (int8_t)(rssi_sum / ap_count) : 0;

    int64_t end_us = esp_timer_get_time();
    result->scan_duration_ms = (uint32_t)((end_us - start_us) / 1000);

    return ESP_OK;
}

void wifi_scan_print_result(const wifi_scan_result_t *result)
{
    if (result == NULL) {
        return;
    }

    ESP_LOGI(TAG, "--- Resultado escaneo WiFi ---");
    ESP_LOGI(TAG, "APs detectados: %u", result->ap_count);
    ESP_LOGI(TAG, "RSSI medio: %d dBm | RSSI max: %d dBm",
             result->rssi_avg, result->rssi_max);
    ESP_LOGI(TAG, "Duracion escaneo: %lu ms", (unsigned long)result->scan_duration_ms);

    // Mostramos solo los canales "clasicos" no solapados (1, 6, 11), que
    // suelen concentrar la mayoria del trafico WiFi en Europa
    ESP_LOGI(TAG, "APs por canal -> ch1: %u | ch6: %u | ch11: %u",
             result->ap_count_per_channel[1],
             result->ap_count_per_channel[6],
             result->ap_count_per_channel[11]);
}
