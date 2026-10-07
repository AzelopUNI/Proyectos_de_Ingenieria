#ifndef WIFI_SCAN_H
#define WIFI_SCAN_H

#include <stdint.h>
#include "esp_err.h"

// Canales WiFi 2.4GHz: usamos indices 1-13 (el 0 no se usa, se deja a 0 siempre)
#define WIFI_SCAN_NUM_CHANNELS 14

/**
 * Resultado agregado de un escaneo WiFi.
 * Esto es lo que se usara como "feature" por segmento/posicion,
 * en vez de guardar la lista completa de APs (ahorra memoria y ancho de banda
 * cuando se suba a Sentilo).
 */
typedef struct {
    uint8_t  ap_count;                                  // numero total de APs visibles
    int8_t   rssi_avg;                                  // RSSI medio en dBm (ej. -67)
    int8_t   rssi_max;                                  // RSSI del AP mas fuerte (señal mas cercana)
    uint8_t  ap_count_per_channel[WIFI_SCAN_NUM_CHANNELS]; // nº de APs detectados en cada canal (1-13)
    uint32_t scan_duration_ms;                          // cuanto tardo el escaneo (util para medir consumo/tiempo)
} wifi_scan_result_t;

/**
 * Inicializa la pila WiFi en modo estacion (STA) SIN conectar a ninguna red.
 * Solo se usa para escanear, no para tener conectividad a internet.
 * Debe llamarse una unica vez al arrancar, antes de usar wifi_scan_perform().
 */
esp_err_t wifi_scan_init(void);

/**
 * Realiza un escaneo WiFi bloqueante (recorre los 13 canales) y rellena 'result'
 * con las metricas agregadas. Pensado para llamarse periodicamente desde el main loop.
 *
 * NOTA de consumo: un escaneo activo completo puede tardar ~1-3 segundos y consume
 * bastante mas que estar en reposo. No llamar con mucha frecuencia (ver main.c).
 */
esp_err_t wifi_scan_perform(wifi_scan_result_t *result);

/**
 * Vuelca el resultado por el log serie (debug). Util mientras no tengais
 * todavia la subida a Sentilo funcionando.
 */
void wifi_scan_print_result(const wifi_scan_result_t *result);

#endif // WIFI_SCAN_H
