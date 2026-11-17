#include <string.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"

#include "rf_scan.h"

static const char *TAG = "rf_scan";

// --- Pines (ESP32-S3) ---
#define RF_PIN_CE    4
#define RF_PIN_CSN   10
#define RF_PIN_SCK   12
#define RF_PIN_MOSI  11
#define RF_PIN_MISO  13
#define RF_SPI_HOST  SPI2_HOST

// --- Registros / comandos del NRF24L01+ ---
#define NRF_CMD_R_REGISTER  0x00
#define NRF_CMD_W_REGISTER  0x20
#define NRF_REG_CONFIG      0x00
#define NRF_REG_EN_AA       0x01
#define NRF_REG_RF_CH       0x05
#define NRF_REG_RF_SETUP    0x06
#define NRF_REG_RPD         0x09

#define NRF_CONFIG_PWR_UP_PRIM_RX  0x03   // PWR_UP=1, PRIM_RX=1
#define NRF_RF_SETUP_2MBPS         0x08   // 2 Mbps (receptor mas ancho, mejor para escanear)

// Barridos completos por escaneo: mas barridos = ratio mas fino pero mas tiempo
#define RF_SCAN_SWEEPS      20
// Tiempo de escucha por canal (>=130 us de settling + 40 us para RPD)
#define RF_LISTEN_US        170

static spi_device_handle_t s_spi;
static bool s_ready = false;

static esp_err_t nrf_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = { NRF_CMD_W_REGISTER | reg, value };
    spi_transaction_t t = { .length = 16, .tx_buffer = tx };
    return spi_device_polling_transmit(s_spi, &t);
}

static esp_err_t nrf_read_reg(uint8_t reg, uint8_t *value)
{
    uint8_t tx[2] = { NRF_CMD_R_REGISTER | reg, 0xFF };
    uint8_t rx[2] = { 0 };
    spi_transaction_t t = { .length = 16, .tx_buffer = tx, .rx_buffer = rx };
    esp_err_t err = spi_device_polling_transmit(s_spi, &t);
    *value = rx[1];
    return err;
}

esp_err_t rf_scan_init(void)
{
    gpio_config_t ce_cfg = {
        .pin_bit_mask = 1ULL << RF_PIN_CE,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&ce_cfg));
    gpio_set_level(RF_PIN_CE, 0);

    spi_bus_config_t bus = {
        .mosi_io_num = RF_PIN_MOSI,
        .miso_io_num = RF_PIN_MISO,
        .sclk_io_num = RF_PIN_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(RF_SPI_HOST, &bus, SPI_DMA_DISABLED));

    spi_device_interface_config_t dev = {
        .clock_speed_hz = 4 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = RF_PIN_CSN,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(RF_SPI_HOST, &dev, &s_spi));

    // Despertar el modulo (power-down -> standby tarda ~1.5-5 ms)
    nrf_write_reg(NRF_REG_CONFIG, NRF_CONFIG_PWR_UP_PRIM_RX);
    esp_rom_delay_us(5000);
    nrf_write_reg(NRF_REG_EN_AA, 0x00);
    nrf_write_reg(NRF_REG_RF_SETUP, NRF_RF_SETUP_2MBPS);

    // Comprobamos que el modulo responde leyendo lo que acabamos de escribir
    uint8_t cfg = 0, setup = 0;
    nrf_read_reg(NRF_REG_CONFIG, &cfg);
    nrf_read_reg(NRF_REG_RF_SETUP, &setup);
    if ((cfg & 0x03) != NRF_CONFIG_PWR_UP_PRIM_RX || (setup & 0x08) != NRF_RF_SETUP_2MBPS) {
        ESP_LOGE(TAG, "NRF24 no responde (CONFIG=0x%02X RF_SETUP=0x%02X). "
                      "Revisa cableado, 3V3 y el condensador de 10uF", cfg, setup);
        return ESP_ERR_NOT_FOUND;
    }

    s_ready = true;
    ESP_LOGI(TAG, "NRF24L01+ detectado e inicializado");
    return ESP_OK;
}

esp_err_t rf_scan_perform(rf_scan_result_t *result)
{
    if (result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(result, 0, sizeof(rf_scan_result_t));
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }

    int64_t start_us = esp_timer_get_time();
    uint16_t hits[RF_SCAN_NUM_CHANNELS] = { 0 };

    for (int sweep = 0; sweep < RF_SCAN_SWEEPS; sweep++) {
        for (int ch = 0; ch < RF_SCAN_NUM_CHANNELS; ch++) {
            nrf_write_reg(NRF_REG_RF_CH, (uint8_t)ch);
            gpio_set_level(RF_PIN_CE, 1);          // escuchar
            esp_rom_delay_us(RF_LISTEN_US);
            gpio_set_level(RF_PIN_CE, 0);

            uint8_t rpd = 0;
            nrf_read_reg(NRF_REG_RPD, &rpd);       // bit0 = portadora > -64 dBm
            if (rpd & 0x01) {
                hits[ch]++;
            }
        }
    }

    for (int ch = 0; ch < RF_SCAN_NUM_CHANNELS; ch++) {
        result->occupancy_ratio_per_channel[ch] = (uint8_t)((hits[ch] * 100) / RF_SCAN_SWEEPS);
    }

    result->valid = 1;
    result->scan_duration_ms = (uint32_t)((esp_timer_get_time() - start_us) / 1000);
    return ESP_OK;
}
