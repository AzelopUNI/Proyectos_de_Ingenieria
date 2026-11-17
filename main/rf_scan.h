#ifndef RF_SCAN_H
#define RF_SCAN_H

#include <stdint.h>
#include "esp_err.h"

// El NRF24L01+ tiene 125 canales de 1 MHz: canal N = 2400 + N MHz
#define RF_SCAN_NUM_CHANNELS 125

/**
 * Resultado de ocupacion de espectro via NRF24L01+.
 * occupancy_ratio_per_channel: % (0-100) de barridos en los que se detecto
 * energia por encima de ~-64 dBm en cada canal.
 */
typedef struct {
    uint8_t  valid;   // 1 si el NRF24 respondio y hay datos reales
    uint8_t  occupancy_ratio_per_channel[RF_SCAN_NUM_CHANNELS];
    uint32_t scan_duration_ms;
} rf_scan_result_t;

/**
 * Inicializa SPI y el NRF24L01+. Devuelve error si el modulo no responde
 * (cableado, alimentacion o condensador). Llamar una sola vez al arrancar.
 */
esp_err_t rf_scan_init(void);

/** Hace varios barridos de los 125 canales y rellena 'result'. Bloqueante (~0.5 s). */
esp_err_t rf_scan_perform(rf_scan_result_t *result);

#endif // RF_SCAN_H
