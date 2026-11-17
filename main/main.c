#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "wifi_scan.h"
#include "gps_scan.h"
#include "rf_scan.h"
#include "sensor_data.h"
#include "sentilo.h"

static const char *TAG = "main";

/* Intervalo entre mediciones */
#define SCAN_INTERVAL_MS 8000

void app_main(void)
{
    ESP_LOGI(TAG,
             "Iniciando proyecto de mapeo radioelectrico");

    /* Inicializar escáner WiFi */
    ESP_ERROR_CHECK(wifi_scan_init());

    /* Inicializar Sentilo */
    ESP_ERROR_CHECK(sentilo_init());

    /* Inicializar GPS */
    bool gps_ok = (gps_init() == ESP_OK);
    if (!gps_ok) {
        ESP_LOGW(TAG, "GPS no disponible");
    }

    /* Inicializar NRF24 */
    bool rf_ok = (rf_scan_init() == ESP_OK);
    if (!rf_ok) {
        ESP_LOGW(TAG,
                 "NRF24 no disponible, se omite barrido RF");
    }

    while (1) {

        wifi_scan_result_t wifi_result;

        esp_err_t err =
            wifi_scan_perform(&wifi_result);

        if (err == ESP_OK) {

            gps_data_t gps = {0};

            if (gps_ok) {
                gps_get(&gps);
            }

            rf_scan_result_t rf = {0};

            if (rf_ok) {
                if (rf_scan_perform(&rf) != ESP_OK) {
                    ESP_LOGW(TAG,
                             "Barrido RF fallido");
                }
            }

            segment_data_t segment;

            sensor_data_build_segment(
                &segment,
                &wifi_result,
                gps_ok ? &gps : NULL,
                rf_ok ? &rf : NULL);

            sensor_data_print_segment(&segment);

            /* Guardar segmento para futura subida */
            sentilo_queue_segment(&segment);

            ESP_LOGI(TAG,
                     "Pendientes Sentilo: %u",
                     (unsigned)sentilo_pending());

            /* Intentar sincronizar cuando toque */
            if (sentilo_should_sync()) {

                ESP_LOGI(TAG,
                         "Sincronizando con Sentilo...");

                esp_err_t sync_err =
                    sentilo_sync();

                if (sync_err != ESP_OK) {
                    ESP_LOGW(TAG,
                             "Sincronizacion fallida");
                }
            }

        } else {

            ESP_LOGW(TAG,
                     "Escaneo WiFi fallido");
        }

        vTaskDelay(
            pdMS_TO_TICKS(SCAN_INTERVAL_MS));
    }
}
