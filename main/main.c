#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "wifi_scan.h"
#include "sensor_data.h"

static const char *TAG = "main";

// Intervalo entre escaneos WiFi completos. Un escaneo activo de los 13
// canales ya tarda 1-3 s y consume bastante, asi que no conviene bajar
// de unos pocos segundos salvo que useis una bici muy lenta o querais
// mas resolucion espacial a costa de bateria.
#define WIFI_SCAN_INTERVAL_MS 8000

void app_main(void)
{
    ESP_LOGI(TAG, "Iniciando proyecto de mapeo radioelectrico (Terrassa/Barcelona)");

    ESP_ERROR_CHECK(wifi_scan_init());

    while (1) {
        wifi_scan_result_t wifi_result;
        esp_err_t err = wifi_scan_perform(&wifi_result);

        if (err == ESP_OK) {
            segment_data_t segment;

            // --- Punto de enganche GPS (pendiente) ---
            // Persona 1: aqui es donde, cuando tengais el modulo GPS,
            // leeriais la posicion/velocidad actual y la pasariais a
            // sensor_data_build_segment() o la asignariais directamente
            // a segment.gps despues de construir el segmento.
            // ej.: gps_data_t gps; gps_read(&gps); segment.gps = gps;

            sensor_data_build_segment(&segment, &wifi_result);

            // --- Punto de enganche NRF24L01+ (pendiente) ---
            // Persona 1: cuando tengais el sensor RF, haced aqui el barrido
            // (p.ej. rf_scan_perform(&rf_result)) y asignad el resultado
            // a segment.rf antes de imprimir/publicar.

            sensor_data_print_segment(&segment);

            // --- Punto de enganche Sentilo (pendiente) ---
            // Persona 2: aqui se llamaria a algo como
            // sentilo_publish(&segment); una vez tengais sentilo.c/.h
            // implementado (buffer en flash + envio por lotes via HTTP).

        } else {
            ESP_LOGW(TAG, "Escaneo WiFi fallido, se reintentara en el siguiente ciclo");
        }

        vTaskDelay(pdMS_TO_TICKS(WIFI_SCAN_INTERVAL_MS));
    }
}
