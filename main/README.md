# Proyecto de mapeo radioeléctrico (Terrassa / Barcelona)

Código base en ESP-IDF para el ESP32-S3. Implementa el escaneo WiFi nativo
(APs visibles, RSSI, ocupación por canal). El NRF24L01+, el GPS y la
publicación a Sentilo están dejados como puntos de extensión (marcados con
`TODO` en el código) para que se puedan añadir sin reestructurar nada.

## Estructura

```
project/
├── CMakeLists.txt          (raíz del proyecto ESP-IDF)
└── main/
    ├── CMakeLists.txt
    ├── main.c              -> ciclo principal, llama al escaneo periódicamente
    ├── wifi_scan.h/.c       -> escaneo WiFi nativo (ya funcional)
    └── sensor_data.h/.c     -> estructura común (WiFi + GPS + RF), con
                                placeholders para lo que falta integrar
```

## Compilar y flashear

Necesitáis el ESP-IDF instalado (v5.x recomendado) y configurado para
ESP32-S3.

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

(Cambiad `/dev/ttyUSB0` por el puerto serie de vuestra placa.)

## Qué hace ahora mismo

Cada `WIFI_SCAN_INTERVAL_MS` (8 segundos por defecto, en `main.c`), el
dispositivo:
1. Escanea los 13 canales WiFi.
2. Calcula APs visibles, RSSI medio/máximo, y ocupación por canal.
3. Construye un `segment_data_t` (con GPS y RF vacíos por ahora).
4. Lo imprime por el log serie.

## Próximos pasos (para cada persona)

- **Persona 1 (sensor/procesado):**
  - Crear `gps.h`/`gps.c` con una función tipo `gps_read(gps_data_t *out)`
    y asignarla a `segment.gps` en `main.c` (ya hay un comentario
    señalando dónde).
  - Crear `rf_scan.h`/`rf_scan.c` para el NRF24L01+, análogo a
    `wifi_scan.h`/`.c`, y asignar el resultado a `segment.rf`.
  - Ajustar `WIFI_SCAN_INTERVAL_MS` y añadir modos de bajo consumo
    (light sleep) una vez el resto funcione.

- **Persona 2 (transmisión a Sentilo):**
  - Crear `sentilo.h`/`sentilo.c` con `sentilo_publish(segment_data_t *segment)`.
  - Añadir buffer en flash (LittleFS/SPIFFS) para acumular segmentos
    mientras no hay WiFi de verdad disponible, y subirlos por lotes.
  - Llamar a `sentilo_publish()` desde el punto marcado en `main.c`.

- **Persona 3 (presentación/Sentilo server):**
  - No necesita tocar este código; trabaja sobre el catálogo y dashboard
    de Sentilo una vez empiecen a llegar datos reales.
