#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

#include "gps_scan.h"

static const char *TAG = "gps_scan";

// --- Pines (ESP32-S3) ---
#define GPS_UART_NUM  UART_NUM_1
#define GPS_PIN_TX    17   // TX del ESP32 -> RX del GPS
#define GPS_PIN_RX    18   // RX del ESP32 <- TX del GPS
#define GPS_BAUD      9600 // por defecto en NEO-6M

#define GPS_RX_BUF_SIZE   1024
#define NMEA_MAX_LEN      128
#define NMEA_MAX_FIELDS   16
#define KNOTS_TO_KMH      1.852f

static gps_data_t s_gps;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_had_fix = false;

// ddmm.mmmm -> grados decimales
static double nmea_to_degrees(const char *field, char hemisphere)
{
    double raw = strtod(field, NULL);
    int deg = (int)(raw / 100.0);
    double d = deg + (raw - deg * 100.0) / 60.0;
    if (hemisphere == 'S' || hemisphere == 'W') {
        d = -d;
    }
    return d;
}

static bool nmea_checksum_ok(const char *line)
{
    const char *star = strchr(line, '*');
    if (star == NULL || strlen(star) < 3) {
        return false;
    }
    uint8_t cs = 0;
    for (const char *p = line + 1; p < star; p++) {
        cs ^= (uint8_t)*p;
    }
    return cs == (uint8_t)strtol(star + 1, NULL, 16);
}

// Procesa una linea NMEA completa. Solo nos interesa la sentencia RMC
// ($GPRMC / $GNRMC): estado del fix, lat, lon y velocidad.
static void process_line(char *line)
{
    if (line[0] != '$' || strlen(line) < 7 || !nmea_checksum_ok(line)) {
        return;
    }
    if (strncmp(line + 3, "RMC", 3) != 0) {
        return;
    }

    *strchr(line, '*') = '\0';

    // Separar campos por comas (respetando los campos vacios)
    char *fields[NMEA_MAX_FIELDS];
    int n = 0;
    char *p = line + 1;
    fields[n++] = p;
    while (*p && n < NMEA_MAX_FIELDS) {
        if (*p == ',') {
            *p = '\0';
            fields[n++] = p + 1;
        }
        p++;
    }
    if (n < 8) {
        return;
    }

    // 2=status(A/V) 3=lat 4=N/S 5=lon 6=E/W 7=speed(nudos)
    gps_data_t g = { 0 };
    if (fields[2][0] == 'A' && fields[3][0] != '\0' && fields[5][0] != '\0') {
        g.valid = 1;
        g.latitude = nmea_to_degrees(fields[3], fields[4][0]);
        g.longitude = nmea_to_degrees(fields[5], fields[6][0]);
        g.speed_kmh = (float)strtod(fields[7], NULL) * KNOTS_TO_KMH;

        if (!s_had_fix) {
            s_had_fix = true;
            ESP_LOGI(TAG, "Primer fix GPS obtenido");
        }
    }

    taskENTER_CRITICAL(&s_lock);
    s_gps = g;
    taskEXIT_CRITICAL(&s_lock);
}

static void gps_task(void *arg)
{
    char line[NMEA_MAX_LEN];
    size_t len = 0;
    uint8_t buf[128];

    while (1) {
        int n = uart_read_bytes(GPS_UART_NUM, buf, sizeof(buf), pdMS_TO_TICKS(200));
        for (int i = 0; i < n; i++) {
            char c = (char)buf[i];
            if (c == '$') {
                len = 0;
                line[len++] = c;
            } else if (c == '\n' || c == '\r') {
                if (len > 0) {
                    line[len] = '\0';
                    process_line(line);
                    len = 0;
                }
            } else if (len > 0 && len < NMEA_MAX_LEN - 1) {
                line[len++] = c;
            } else {
                len = 0; // linea demasiado larga o basura
            }
        }
    }
}

esp_err_t gps_init(void)
{
    uart_config_t cfg = {
        .baud_rate = GPS_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(GPS_UART_NUM, GPS_RX_BUF_SIZE, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(GPS_UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(GPS_UART_NUM, GPS_PIN_TX, GPS_PIN_RX,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    if (xTaskCreate(gps_task, "gps_task", 4096, NULL, 5, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "GPS iniciado en UART1 (RX=%d TX=%d, %d baud)", GPS_PIN_RX, GPS_PIN_TX, GPS_BAUD);
    return ESP_OK;
}

void gps_get(gps_data_t *out)
{
    if (out == NULL) {
        return;
    }
    taskENTER_CRITICAL(&s_lock);
    *out = s_gps;
    taskEXIT_CRITICAL(&s_lock);
}
