#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"

#include "sensor_data.h"

static const char *TAG = "sensor_data";

void sensor_data_build_segment(segment_data_t *segment,
                               const wifi_scan_result_t *wifi_result,
                               const gps_data_t *gps,
                               const rf_scan_result_t *rf)
{
    if (segment == NULL || wifi_result == NULL) {
        return;
    }

    memset(segment, 0, sizeof(segment_data_t));

    segment->timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
    segment->wifi = *wifi_result;

    // Si un sensor no esta disponible, se queda a 0 / valid=0
    if (gps != NULL) {
        segment->gps = *gps;
    }
    if (rf != NULL) {
        segment->rf = *rf;
    }
}

void sensor_data_print_segment(const segment_data_t *segment)
{
    if (segment == NULL) {
        return;
    }

    ESP_LOGI(TAG, "===== Segmento @ %lu ms =====", (unsigned long)segment->timestamp_ms);

    if (segment->gps.valid) {
        ESP_LOGI(TAG, "GPS: lat=%.6f lon=%.6f speed=%.1f km/h",
                 segment->gps.latitude, segment->gps.longitude, segment->gps.speed_kmh);
    } else {
        ESP_LOGI(TAG, "GPS: sin fix (necesita cielo abierto, puede tardar unos minutos)");
    }

    wifi_scan_print_result(&segment->wifi);

    if (segment->rf.valid) {
        uint32_t sum = 0;
        uint8_t max_occ = 0;
        int max_ch = 0;
        for (int i = 0; i < RF_SCAN_NUM_CHANNELS; i++) {
            uint8_t occ = segment->rf.occupancy_ratio_per_channel[i];
            sum += occ;
            if (occ > max_occ) {
                max_occ = occ;
                max_ch = i;
            }
        }
        ESP_LOGI(TAG, "RF (NRF24): ocupacion media %lu %% | canal mas ocupado: %d (%d MHz) con %u %% | %lu ms",
                 (unsigned long)(sum / RF_SCAN_NUM_CHANNELS), max_ch, 2400 + max_ch,
                 max_occ, (unsigned long)segment->rf.scan_duration_ms);
    } else {
        ESP_LOGI(TAG, "RF (NRF24): no disponible");
    }

    ESP_LOGI(TAG, "================================");
}
