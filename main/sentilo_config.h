#ifndef SENTILO_CONFIG_H
#define SENTILO_CONFIG_H

// ============================================================
//  CONFIGURACION DE SENTILO  -> rellena estos valores
//  (no subas este fichero a un repositorio publico: lleva el token)
// ============================================================

// WiFi al que se conecta la placa SOLO para subir datos (p.ej. hotspot del movil).
// El ESP32-S3 solo soporta 2.4 GHz: en iPhone activa "Maximizar compatibilidad".
#define SENTILO_WIFI_SSID      "RedmiNote145G"
#define SENTILO_WIFI_PASSWORD  "cositassi"

// URL base de la API de Sentilo, SIN barra final. Puerto habitual: 8081
#define SENTILO_API_URL        "http://147.83.83.21:8081"
#define SENTILO_USE_HTTPS      1      // 1 si SENTILO_API_URL empieza por https://

// Identificador del provider y su token (cabecera IDENTITY_KEY)
#define SENTILO_PROVIDER       "grup_6-103@ProyectoUltron"
#define SENTILO_TOKEN          "5b4acea3c3a90215a6edcff4f8740f617875c8a19a4b68cbe348f62b42c42f24"

// Prefijo de los IDs de sensor (deben ser unicos dentro del provider).
// Sensores finales: bici01_wifi_ap_count, bici01_rf_spectrum, ...
#define SENTILO_SENSOR_PREFIX  "bici01_"

// Servidor NTP para poner la hora real (necesario: las medidas se guardan con
// tiempo desde el arranque y se convierten a hora UTC al subirlas)
#define SENTILO_NTP_SERVER     "pool.ntp.org"

// 1 = solo guarda segmentos con fix GPS (usar en los recorridos reales)
// 0 = guarda todo (util para probar en interior)
#define SENTILO_ONLY_WITH_GPS_FIX      0

// --- Buffer y envio por lotes ---
#define SENTILO_BUFFER_SEGMENTS        200      // segmentos en RAM (~40 KB). Si se llena, se pisa el mas antiguo
#define SENTILO_SYNC_EVERY_N_SEGMENTS  15       // intentar subir cuando haya al menos N pendientes
#define SENTILO_MIN_SYNC_INTERVAL_MS   120000   // minimo entre intentos (evita bloquear medidas si no hay WiFi)
#define SENTILO_BATCH_SEGMENTS         8        // segmentos por peticion HTTP
#define SENTILO_MAX_BATCHES_PER_SYNC   25
#define SENTILO_BODY_BUF_SIZE          16384    // bytes del cuerpo JSON de cada peticion

#define SENTILO_WIFI_TIMEOUT_MS        15000
#define SENTILO_WIFI_MAX_RETRIES       2
#define SENTILO_HTTP_TIMEOUT_MS        10000
#define SENTILO_TASK_STACK_SIZE        10240    // el cliente HTTP(S) necesita bastante pila

#endif // SENTILO_CONFIG_H
