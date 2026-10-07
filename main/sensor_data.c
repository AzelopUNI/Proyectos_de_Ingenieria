#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"

#include "sensor_data.h"

static const char *TAG = "sensor_data";

/**
 * Construye un segmento combinando los datos que ya tenemos (WiFi) con
 * placeholders vacios para GPS y RF (NRF24), de forma que el resto del
 * codigo (y el futuro envio a Sentilo) ya pueda trabajar con la
 * estructura completa aunque todavia falten sensores por conectar.
 */
void sensor_data_build_segment(segment_data_t *segment, const wifi_scan_result_t *wifi_result)
{
    if (segment == NULL || wifi_result == NULL) {
        return;
    }

    memset(segment, 0, sizeof(segment_data_t));

    segment->timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
    segment->wifi = *wifi_result;

    // GPS y RF se quedan a 0 / valid=0 hasta que se implementen esos sensores.
    // Persona 1: cuando tengais el GPS, rellenad segment->gps aqui (o en un
    // gps_scan.c nuevo que devuelva un gps_data_t y se asigne igual que wifi).
    // Cuando tengais el NRF24, lo mismo con segment->rf.
}

/**
 * Vuelca un segmento completo por el log. Sirve como sustituto temporal
 * de la publicacion en Sentilo (Persona 2 reemplazara/complementara esto
 * con sentilo_publish(segment) en su propio sentilo.c).
 */
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
        ESP_LOGI(TAG, "GPS: sin fix (pendiente de integrar)");
    }

    wifi_scan_print_result(&segment->wifi);

    if (segment->rf.valid) {
        ESP_LOGI(TAG, "RF (NRF24): datos disponibles");
        // TODO: cuando se implemente, aqui se podria imprimir un resumen
        // (p.ej. ocupacion media, canales mas ocupados, etc.)
    } else {
        ESP_LOGI(TAG, "RF (NRF24): pendiente de integrar");
    }

    ESP_LOGI(TAG, "================================");
}
