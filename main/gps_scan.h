#ifndef GPS_SCAN_H
#define GPS_SCAN_H

#include <stdint.h>
#include "esp_err.h"

/** Posicion GPS asociada a un segmento de medida. */
typedef struct {
    uint8_t valid;       // 1 si hay fix GPS valido, 0 si no
    double  latitude;    // grados decimales (S negativo)
    double  longitude;   // grados decimales (W negativo)
    float   speed_kmh;
} gps_data_t;

/**
 * Inicializa la UART del GPS y lanza una tarea que lee y parsea NMEA
 * continuamente en segundo plano. Llamar una sola vez al arrancar.
 */
esp_err_t gps_init(void);

/** Copia la ultima posicion conocida en 'out' (valid=0 si aun no hay fix). */
void gps_get(gps_data_t *out);

#endif // GPS_SCAN_H
