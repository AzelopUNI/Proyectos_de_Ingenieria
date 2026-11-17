#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#if SENTILO_USE_HTTPS
#include "esp_crt_bundle.h"
#endif

#include "sentilo_config.h"
#include "sentilo.h"

static const char *TAG = "sentilo";

/* ------------------------------------------------------------------ */
/*  Sensores publicados (cada uno debe existir en el catalogo Sentilo)  */
/* ------------------------------------------------------------------ */
typedef enum {
    S_WIFI_APS = 0,    // NUMBER: APs detectados
    S_WIFI_RSSI_AVG,   // NUMBER: dBm
    S_WIFI_RSSI_MAX,   // NUMBER: dBm
    S_WIFI_CHANNELS,   // TEXT:   APs en canales 1..13 separados por comas
    S_RF_OCC_AVG,      // NUMBER: % ocupacion media NRF24
    S_RF_OCC_MAX,      // NUMBER: % del canal mas ocupado
    S_RF_SPECTRUM,     // TEXT:   % ocupacion de los 125 canales NRF24
    S_COUNT
} sensor_idx_t;

static const char *const SENSOR_NAMES[S_COUNT] = {
    "wifi_ap_count", "wifi_rssi_avg", "wifi_rssi_max", "wifi_channels",
    "rf_occupancy_avg", "rf_occupancy_max", "rf_spectrum",
};

/* ------------------------------------------------------------------ */
/*  Buffer circular en RAM                                              */
/* ------------------------------------------------------------------ */
static segment_data_t s_buf[SENTILO_BUFFER_SEGMENTS];
static size_t s_head = 0;    // siguiente posicion de escritura
static size_t s_count = 0;   // segmentos pendientes

static bool s_attempted = false;
static int64_t s_last_attempt_ms = 0;

void sentilo_queue_segment(const segment_data_t *segment)
{
    if (segment == NULL) {
        return;
    }
#if SENTILO_ONLY_WITH_GPS_FIX
    if (!segment->gps.valid) {
        ESP_LOGI(TAG, "Segmento descartado: sin fix GPS");
        return;
    }
#endif
    s_buf[s_head] = *segment;
    s_head = (s_head + 1) % SENTILO_BUFFER_SEGMENTS;
    if (s_count < SENTILO_BUFFER_SEGMENTS) {
        s_count++;
    } else {
        ESP_LOGW(TAG, "Buffer lleno: se sobrescribe el segmento mas antiguo");
    }
}

size_t sentilo_pending(void)
{
    return s_count;
}

bool sentilo_should_sync(void)
{
    if (s_count < SENTILO_SYNC_EVERY_N_SEGMENTS) {
        return false;
    }
    int64_t now_ms = esp_timer_get_time() / 1000;
    if (s_attempted && (now_ms - s_last_attempt_ms) < SENTILO_MIN_SYNC_INTERVAL_MS) {
        return false;
    }
    return true;
}

/* ------------------------------------------------------------------ */
/*  WiFi (conectar solo durante la subida)                              */
/* ------------------------------------------------------------------ */
#define BIT_GOT_IP  BIT0
#define BIT_FAIL    BIT1

static EventGroupHandle_t s_evt;
static volatile bool s_syncing = false;
static int s_retries = 0;

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (!s_syncing) {
        return;   // fuera de una subida ignoramos los eventos
    }
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retries < SENTILO_WIFI_MAX_RETRIES) {
            s_retries++;
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_evt, BIT_FAIL);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_evt, BIT_GOT_IP);
    }
}

static void copy_cfg_str(uint8_t *dst, size_t dst_size, const char *src)
{
    size_t n = strlen(src);
    if (n >= dst_size) {
        n = dst_size - 1;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

esp_err_t sentilo_init(void)
{
    s_evt = xEventGroupCreate();
    if (s_evt == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL));

    wifi_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    copy_cfg_str(cfg.sta.ssid, sizeof(cfg.sta.ssid), SENTILO_WIFI_SSID);
    copy_cfg_str(cfg.sta.password, sizeof(cfg.sta.password), SENTILO_WIFI_PASSWORD);
    cfg.sta.threshold.authmode = (strlen(SENTILO_WIFI_PASSWORD) > 0) ? WIFI_AUTH_WPA2_PSK
                                                                     : WIFI_AUTH_OPEN;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));

    if (strcmp(SENTILO_WIFI_SSID, "TU_HOTSPOT") == 0 || strcmp(SENTILO_PROVIDER, "TU_PROVIDER_ID") == 0) {
        ESP_LOGW(TAG, "sentilo_config.h sin rellenar: las subidas fallaran");
    }
    ESP_LOGI(TAG, "Sentilo listo: provider '%s', lotes de %d segmentos, subida cada >=%d segmentos",
             SENTILO_PROVIDER, SENTILO_BATCH_SEGMENTS, SENTILO_SYNC_EVERY_N_SEGMENTS);
    return ESP_OK;
}

static bool wifi_connect(void)
{
    xEventGroupClearBits(s_evt, BIT_GOT_IP | BIT_FAIL);
    s_retries = 0;
    s_syncing = true;

    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_connect fallo: %s", esp_err_to_name(err));
        s_syncing = false;
        return false;
    }

    EventBits_t bits = xEventGroupWaitBits(s_evt, BIT_GOT_IP | BIT_FAIL, pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(SENTILO_WIFI_TIMEOUT_MS));
    if (bits & BIT_GOT_IP) {
        ESP_LOGI(TAG, "Conectado a '%s'", SENTILO_WIFI_SSID);
        return true;
    }

    ESP_LOGW(TAG, "No se pudo conectar a '%s' (fuera de cobertura o clave incorrecta)", SENTILO_WIFI_SSID);
    s_syncing = false;
    esp_wifi_disconnect();
    return false;
}

static void wifi_disconnect(void)
{
    s_syncing = false;
    esp_wifi_disconnect();
}

/* ------------------------------------------------------------------ */
/*  Hora real (NTP)                                                     */
/* ------------------------------------------------------------------ */
static bool time_is_valid(void)
{
    return time(NULL) > 1700000000;   // posterior a nov-2023 => ya sincronizada
}

static bool sync_time(void)
{
    if (time_is_valid()) {
        return true;
    }
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG(SENTILO_NTP_SERVER);
    if (esp_netif_sntp_init(&cfg) != ESP_OK) {
        return false;
    }
    esp_err_t err = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000));
    esp_netif_sntp_deinit();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo obtener la hora por NTP (%s)", SENTILO_NTP_SERVER);
        return false;
    }
    ESP_LOGI(TAG, "Hora sincronizada por NTP");
    return true;
}

/* Sentilo espera "dd/MM/yyyyTHH:mm:ss" (UTC por defecto) */
static void format_ts(time_t t, char *out, size_t n)
{
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(out, n, "%d/%m/%YT%H:%M:%S", &tm);
}

/* ------------------------------------------------------------------ */
/*  Construccion del JSON                                               */
/* ------------------------------------------------------------------ */
typedef struct {
    char *buf;
    size_t cap;
    size_t len;
    bool overflow;
} sb_t;

static void sb_printf(sb_t *b, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void sb_printf(sb_t *b, const char *fmt, ...)
{
    if (b->overflow) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(b->buf + b->len, b->cap - b->len, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= b->cap - b->len) {
        b->overflow = true;
        return;
    }
    b->len += (size_t)n;
}

static bool csv_u8(char *out, size_t n, const uint8_t *vals, size_t first, size_t last)
{
    size_t pos = 0;
    for (size_t i = first; i <= last; i++) {
        int w = snprintf(out + pos, n - pos, "%s%u", (i > first) ? "," : "", (unsigned)vals[i]);
        if (w < 0 || (size_t)w >= n - pos) {
            return false;
        }
        pos += (size_t)w;
    }
    return true;
}

/* Devuelve false si ese sensor no tiene dato en este segmento (no se publica). */
static bool sensor_value(int idx, const segment_data_t *s, char *out, size_t n)
{
    switch (idx) {
    case S_WIFI_APS:
        snprintf(out, n, "%u", (unsigned)s->wifi.ap_count);
        return true;
    case S_WIFI_RSSI_AVG:
        if (s->wifi.ap_count == 0) return false;
        snprintf(out, n, "%d", (int)s->wifi.rssi_avg);
        return true;
    case S_WIFI_RSSI_MAX:
        if (s->wifi.ap_count == 0) return false;
        snprintf(out, n, "%d", (int)s->wifi.rssi_max);
        return true;
    case S_WIFI_CHANNELS:
        return csv_u8(out, n, s->wifi.ap_count_per_channel, 1, WIFI_SCAN_NUM_CHANNELS - 1);
    case S_RF_OCC_AVG:
    case S_RF_OCC_MAX: {
        if (!s->rf.valid) return false;
        unsigned sum = 0, max = 0;
        for (int i = 0; i < RF_SCAN_NUM_CHANNELS; i++) {
            unsigned v = s->rf.occupancy_ratio_per_channel[i];
            sum += v;
            if (v > max) max = v;
        }
        snprintf(out, n, "%u", (idx == S_RF_OCC_AVG) ? (sum / RF_SCAN_NUM_CHANNELS) : max);
        return true;
    }
    case S_RF_SPECTRUM:
        if (!s->rf.valid) return false;
        return csv_u8(out, n, s->rf.occupancy_ratio_per_channel, 0, RF_SCAN_NUM_CHANNELS - 1);
    default:
        return false;
    }
}

/*
 * Cuerpo para PUT /data/<provider>:
 * {"sensors":[{"sensor":"id","observations":[{"value":"..","timestamp":"..","location":"lat lon"},...]},...]}
 * 'n' segmentos a partir de la posicion 'tail' del buffer circular.
 */
static void build_body(sb_t *b, size_t tail, size_t n, time_t now_real, int64_t now_boot_ms)
{
    char val[520];
    char ts[24];
    bool first_sensor = true;

    sb_printf(b, "{\"sensors\":[");
    for (int s = 0; s < S_COUNT; s++) {
        size_t mark = b->len;
        if (!first_sensor) {
            sb_printf(b, ",");
        }
        sb_printf(b, "{\"sensor\":\"%s%s\",\"observations\":[", SENTILO_SENSOR_PREFIX, SENSOR_NAMES[s]);

        bool first_obs = true;
        for (size_t i = 0; i < n; i++) {
            const segment_data_t *seg = &s_buf[(tail + i) % SENTILO_BUFFER_SEGMENTS];
            if (!sensor_value(s, seg, val, sizeof(val))) {
                continue;
            }
            // Edad del segmento (aritmetica uint32 => correcta aunque el contador de ms de la vuelta)
            uint32_t age_ms = (uint32_t)now_boot_ms - seg->timestamp_ms;
            format_ts(now_real - (time_t)(age_ms / 1000), ts, sizeof(ts));

            if (!first_obs) {
                sb_printf(b, ",");
            }
            sb_printf(b, "{\"value\":\"%s\",\"timestamp\":\"%s\"", val, ts);
            if (seg->gps.valid) {
                sb_printf(b, ",\"location\":\"%.6f %.6f\"", seg->gps.latitude, seg->gps.longitude);
            }
            sb_printf(b, "}");
            first_obs = false;
        }

        if (first_obs) {            // este sensor no tiene observaciones: no se incluye
            b->len = mark;
            if (!b->overflow) b->buf[mark] = '\0';
        } else {
            sb_printf(b, "]}");
            first_sensor = false;
        }
    }
    sb_printf(b, "]}");
}

/* ------------------------------------------------------------------ */
/*  HTTP                                                                */
/* ------------------------------------------------------------------ */
static esp_err_t http_put_data(const char *body, size_t len, int *status)
{
    esp_http_client_config_t cfg = {
        .url = SENTILO_API_URL "/data/" SENTILO_PROVIDER,
        .method = HTTP_METHOD_PUT,
        .timeout_ms = SENTILO_HTTP_TIMEOUT_MS,
#if SENTILO_USE_HTTPS
        .crt_bundle_attach = esp_crt_bundle_attach,
#endif
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return ESP_FAIL;
    }
    esp_http_client_set_header(client, "IDENTITY_KEY", SENTILO_TOKEN);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, (int)len);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        *status = esp_http_client_get_status_code(client);
    }
    esp_http_client_cleanup(client);
    return err;
}

static esp_err_t upload_pending(void)
{
    char *body = malloc(SENTILO_BODY_BUF_SIZE);
    if (body == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t result = ESP_OK;
    size_t batch = SENTILO_BATCH_SEGMENTS;
    int batches_done = 0;
    size_t sent_total = 0;

    while (s_count > 0 && batches_done < SENTILO_MAX_BATCHES_PER_SYNC) {
        size_t n = (s_count < batch) ? s_count : batch;
        size_t tail = (s_head + SENTILO_BUFFER_SEGMENTS - s_count) % SENTILO_BUFFER_SEGMENTS;

        sb_t b = { .buf = body, .cap = SENTILO_BODY_BUF_SIZE, .len = 0, .overflow = false };
        build_body(&b, tail, n, time(NULL), esp_timer_get_time() / 1000);

        if (b.overflow) {           // el lote no cabe: probamos con la mitad
            if (n > 1) {
                batch = n / 2;
                continue;
            }
            ESP_LOGE(TAG, "Un solo segmento no cabe en SENTILO_BODY_BUF_SIZE");
            result = ESP_ERR_NO_MEM;
            break;
        }

        int status = 0;
        esp_err_t err = http_put_data(body, b.len, &status);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Fallo de red al enviar: %s", esp_err_to_name(err));
            result = err;
            break;
        }
        if (status < 200 || status >= 300) {
            ESP_LOGE(TAG, "Sentilo rechazo el lote (HTTP %d). Revisa token, provider y que los "
                          "sensores esten registrados en el catalogo", status);
            result = ESP_FAIL;
            break;
        }

        s_count -= n;
        sent_total += n;
        batches_done++;
        ESP_LOGI(TAG, "Lote enviado: %u segmentos (HTTP %d), quedan %u",
                 (unsigned)n, status, (unsigned)s_count);
    }

    free(body);
    ESP_LOGI(TAG, "Subida terminada: %u segmentos enviados, %u pendientes",
             (unsigned)sent_total, (unsigned)s_count);
    return result;
}

/* ------------------------------------------------------------------ */
/*  Sincronizacion (en una tarea propia: el cliente HTTP necesita mas   */
/*  pila de la que tiene la tarea principal)                            */
/* ------------------------------------------------------------------ */
typedef struct {
    esp_err_t result;
    TaskHandle_t waiter;
} sync_ctx_t;

static esp_err_t sync_blocking(void)
{
    if (!wifi_connect()) {
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t result = ESP_ERR_INVALID_STATE;
    if (sync_time()) {
        result = upload_pending();
    }
    wifi_disconnect();
    return result;
}

static void sync_task(void *arg)
{
    sync_ctx_t *ctx = (sync_ctx_t *)arg;
    ctx->result = sync_blocking();
    xTaskNotifyGive(ctx->waiter);
    vTaskDelete(NULL);
}

esp_err_t sentilo_sync(void)
{
    if (s_count == 0) {
        return ESP_OK;
    }

    s_attempted = true;
    s_last_attempt_ms = esp_timer_get_time() / 1000;
    ESP_LOGI(TAG, "Intentando subir %u segmentos a Sentilo...", (unsigned)s_count);

    sync_ctx_t ctx = { .result = ESP_FAIL, .waiter = xTaskGetCurrentTaskHandle() };
    if (xTaskCreate(sync_task, "sentilo_sync", SENTILO_TASK_STACK_SIZE, &ctx, 5, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    if (ctx.result != ESP_OK) {
        ESP_LOGW(TAG, "Subida fallida (%s): los datos se conservan para el siguiente intento",
                 esp_err_to_name(ctx.result));
    }
    return ctx.result;
}
