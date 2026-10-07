#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <stdint.h>
#include "wifi_scan.h"

/**
 * Posicion GPS asociada a un segmento de medida.
 * De momento esta estructura existe pero NO se rellena todavia
 * (Persona 1 la completara cuando se integre el modulo GPS).
 *
 * 'valid' indica si hay fix GPS valido en el momento de la medida;
 * mientras no haya GPS, dejadlo siempre en 0 y lat/lon a 0.
 */
typedef struct {
    uint8_t valid;       // 1 si hay fix GPS valido, 0 si no
    double  latitude;
    double  longitude;
    float   speed_kmh;
} gps_data_t;

/**
 * Resultado de ocupacion de espectro vía NRF24L01+ (pendiente de implementar).
 * Se deja ya definida la estructura para que, cuando se añada el sensor,
 * solo haya que escribir rf_scan.c/.h y rellenar estos campos, sin tener
 * que tocar sensor_data.h ni main.c.
 *
 * occupancy_ratio_per_channel: fraccion (0-100 %) de barridos en los que
 * se detecto energia por encima del umbral, uno por cada uno de los 125
 * canales de 1 MHz del NRF24 (2400-2525 MHz aprox).
 */
#define RF_SCAN_NUM_CHANNELS 125

typedef struct {
    uint8_t valid; // 1 cuando el NRF24 este integrado y haya datos reales
    uint8_t occupancy_ratio_per_channel[RF_SCAN_NUM_CHANNELS];
} rf_scan_result_t;

/**
 * Segmento completo: lo que realmente se va a publicar en Sentilo.
 * Agrupa todas las fuentes de datos de un mismo punto/momento del recorrido.
 */
typedef struct {
    uint32_t timestamp_ms;        // instante de la medida (desde el boot; GPS dara tiempo real mas adelante)
    gps_data_t gps;                // TODO: rellenar cuando se integre el GPS
    wifi_scan_result_t wifi;       // ya funcional
    rf_scan_result_t rf;           // TODO: rellenar cuando se integre el NRF24L01+
} segment_data_t;

/**
 * Combina el resultado del escaneo WiFi (y, mas adelante, GPS/RF) en un
 * unico segment_data_t listo para imprimir o para publicar en Sentilo.
 */
void sensor_data_build_segment(segment_data_t *segment, const wifi_scan_result_t *wifi_result);

/**
 * Imprime el segmento por el log serie (debug / sustituto temporal de Sentilo).
 */
void sensor_data_print_segment(const segment_data_t *segment);

#endif // SENSOR_DATA_H
