#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "relinow_espnow.h"

#define RELINOW_CHANNEL 1u
#define RELINOW_TX_PERIOD_MS 1000u
#define RELINOW_STARTUP_GUARD_MS 2500u

static const char* TAG = "relinow_a";

/* ESP32 B STA MAC address (COM4). */
static const uint8_t PEER_MAC[6] = {0xAC, 0x15, 0x18, 0xE6, 0x5B, 0xD0};

static relinow_espnow_node_t g_node;
static SemaphoreHandle_t g_node_lock;

static uint32_t now_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void print_mac(const char* prefix, const uint8_t mac[6]) {
    ESP_LOGI(TAG, "%s %02X:%02X:%02X:%02X:%02X:%02X", prefix,
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void on_message(
    const uint8_t src_mac[6],
    uint8_t channel_id,
    uint16_t seq_id,
    const uint8_t* payload,
    uint16_t payload_len,
    void* user_ctx
) {
    (void)user_ctx;
    ESP_LOGI(TAG, "RX ch=%u seq=%u len=%u from %02X:%02X:%02X:%02X:%02X:%02X : %.*s",
             channel_id,
             seq_id,
             payload_len,
             src_mac[0], src_mac[1], src_mac[2], src_mac[3], src_mac[4], src_mac[5],
             payload_len,
             (const char*)payload);
}

static void on_tx_event(relinow_reliable_tx_event_t event, uint16_t seq_id, void* user_ctx) {
    (void)user_ctx;
    if (event == RELINOW_RELIABLE_TX_NEW) {
        ESP_LOGI(TAG, "TX new seq=%u", seq_id);
    } else if (event == RELINOW_RELIABLE_TX_RETRANSMIT) {
        ESP_LOGW(TAG, "TX retransmit seq=%u", seq_id);
    } else if (event == RELINOW_RELIABLE_TX_FAILED) {
        ESP_LOGE(TAG, "TX failed seq=%u", seq_id);
    }
}

static const char* send_status_str(esp_now_send_status_t status) {
    return (status == ESP_NOW_SEND_SUCCESS) ? "SUCCESS" : "FAIL";
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
static void send_cb(const esp_now_send_info_t* tx_info, esp_now_send_status_t status) {
    if (tx_info == NULL || tx_info->des_addr == NULL) {
        return;
    }
    ESP_LOGI(TAG, "ESPNOW tx status=%s(%d) dst=%02X:%02X:%02X:%02X:%02X:%02X",
             send_status_str(status),
             (int)status,
             tx_info->des_addr[0], tx_info->des_addr[1], tx_info->des_addr[2],
             tx_info->des_addr[3], tx_info->des_addr[4], tx_info->des_addr[5]);
    if (xSemaphoreTake(g_node_lock, pdMS_TO_TICKS(10)) == pdTRUE) {
        relinow_espnow_on_send_status(&g_node, tx_info->des_addr, status);
        xSemaphoreGive(g_node_lock);
    }
}
#else
static void send_cb(const uint8_t* mac_addr, esp_now_send_status_t status) {
    if (mac_addr == NULL) {
        return;
    }
    ESP_LOGI(TAG, "ESPNOW tx status=%s(%d) dst=%02X:%02X:%02X:%02X:%02X:%02X",
             send_status_str(status),
             (int)status,
             mac_addr[0], mac_addr[1], mac_addr[2],
             mac_addr[3], mac_addr[4], mac_addr[5]);
    if (xSemaphoreTake(g_node_lock, pdMS_TO_TICKS(10)) == pdTRUE) {
        relinow_espnow_on_send_status(&g_node, mac_addr, status);
        xSemaphoreGive(g_node_lock);
    }
}
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static void recv_cb(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (info == NULL || info->src_addr == NULL || data == NULL || len <= 0) {
        return;
    }
    if (xSemaphoreTake(g_node_lock, pdMS_TO_TICKS(10)) == pdTRUE) {
        (void)relinow_espnow_on_receive(&g_node, info->src_addr, data, (uint16_t)len, now_ms());
        xSemaphoreGive(g_node_lock);
    }
}
#else
static void recv_cb(const uint8_t* mac_addr, const uint8_t* data, int len) {
    if (mac_addr == NULL || data == NULL || len <= 0) {
        return;
    }
    if (xSemaphoreTake(g_node_lock, pdMS_TO_TICKS(10)) == pdTRUE) {
        (void)relinow_espnow_on_receive(&g_node, mac_addr, data, (uint16_t)len, now_ms());
        xSemaphoreGive(g_node_lock);
    }
}
#endif

static esp_err_t wifi_espnow_init(void) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    uint8_t primary = 0;
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_channel(RELINOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_get_channel(&primary, &second));
    ESP_LOGI(TAG, "WiFi STA channel=%u (target=%u)", primary, RELINOW_CHANNEL);

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(recv_cb));

    {
        esp_now_peer_info_t peer;
        memset(&peer, 0, sizeof(peer));
        memcpy(peer.peer_addr, PEER_MAC, 6u);
        peer.channel = RELINOW_CHANNEL;
        peer.ifidx = WIFI_IF_STA;
        peer.encrypt = false;
        ESP_ERROR_CHECK(esp_now_add_peer(&peer));
    }

    return ESP_OK;
}

void app_main(void) {
    uint8_t local_mac[6];
    uint32_t last_send = 0u;
    uint32_t counter = 0u;
    uint32_t startup_guard_until = 0u;
    relinow_espnow_config_t cfg;
    esp_err_t nvs_rc;

    nvs_rc = nvs_flash_init();
    if (nvs_rc == ESP_ERR_NVS_NO_FREE_PAGES || nvs_rc == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_rc = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_rc);

    g_node_lock = xSemaphoreCreateMutex();
    if (g_node_lock == NULL) {
        ESP_LOGE(TAG, "mutex alloc failed");
        return;
    }

    ESP_ERROR_CHECK(wifi_espnow_init());
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, local_mac));
    print_mac("Local MAC:", local_mac);
    print_mac("Peer  MAC:", PEER_MAC);

    relinow_espnow_default_config(&cfg);
    memcpy(cfg.peer_mac, PEER_MAC, 6u);
    cfg.channel_id = 1u;
    cfg.on_message = on_message;
    cfg.on_tx_event = on_tx_event;

    ESP_ERROR_CHECK(relinow_espnow_init(&g_node, &cfg));
    startup_guard_until = now_ms() + RELINOW_STARTUP_GUARD_MS;
    ESP_LOGI(TAG, "TX startup guard: %lu ms", (unsigned long)RELINOW_STARTUP_GUARD_MS);

    while (1) {
        uint32_t now = now_ms();

        if (now >= startup_guard_until && (now - last_send) >= RELINOW_TX_PERIOD_MS) {
            char msg[64];
            int n = snprintf(msg, sizeof(msg), "A->B reliable msg #%lu", (unsigned long)counter++);
            if (n > 0) {
                if (xSemaphoreTake(g_node_lock, pdMS_TO_TICKS(20)) == pdTRUE) {
                    esp_err_t rc = relinow_espnow_send_reliable(&g_node, (const uint8_t*)msg, (uint16_t)n, now);
                    if (rc != ESP_OK) {
                        ESP_LOGE(TAG, "send failed: %s", esp_err_to_name(rc));
                    }
                    xSemaphoreGive(g_node_lock);
                }
            }
            last_send = now;
        }

        if (xSemaphoreTake(g_node_lock, pdMS_TO_TICKS(10)) == pdTRUE) {
            (void)relinow_espnow_poll(&g_node, now);
            xSemaphoreGive(g_node_lock);
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
