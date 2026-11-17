#ifndef SENTILO_H
#define SENTILO_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "sensor_data.h"

/**
 * Prepara el envio a Sentilo: configura el SSID/password del WiFi de subida y
 * registra los handlers de eventos. Llamar DESPUES de wifi_scan_init().
 */
esp_err_t sentilo_init(void);

/** Guarda un segmento en el buffer en RAM (pendiente de subir). */
void sentilo_queue_segment(const segment_data_t *segment);

/** Numero de segmentos pendientes de subir. */
size_t sentilo_pending(void);

/** true si toca intentar una subida (suficientes pendientes y pasado el intervalo minimo). */
bool sentilo_should_sync(void);

/**
 * Conecta al WiFi de subida, pone la hora por NTP, envia los segmentos pendientes
 * por lotes (PUT /data/<provider>) y desconecta. Bloqueante (hasta ~30 s).
 * Si falla, los datos se conservan para el siguiente intento.
 */
esp_err_t sentilo_sync(void);

#endif // SENTILO_H
