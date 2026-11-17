#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <stdint.h>
#include "wifi_scan.h"
#include "gps_scan.h"   // gps_data_t
#include "rf_scan.h"    // rf_scan_result_t

/**
 * Segmento completo: lo que realmente se va a publicar en Sentilo.
 * Agrupa todas las fuentes de datos de un mismo punto/momento del recorrido.
 */
typedef struct {
    uint32_t timestamp_ms;        // instante de la medida (desde el boot)
    gps_data_t gps;
    wifi_scan_result_t wifi;
    rf_scan_result_t rf;
} segment_data_t;

/**
 * Combina WiFi, GPS y RF en un unico segment_data_t.
 * 'gps' y 'rf' pueden ser NULL si ese sensor no esta disponible.
 */
void sensor_data_build_segment(segment_data_t *segment,
                               const wifi_scan_result_t *wifi_result,
                               const gps_data_t *gps,
                               const rf_scan_result_t *rf);

/** Imprime el segmento por el log serie (debug / sustituto temporal de Sentilo). */
void sensor_data_print_segment(const segment_data_t *segment);

#endif // SENSOR_DATA_H
