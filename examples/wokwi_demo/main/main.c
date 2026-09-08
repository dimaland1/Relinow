#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_now.h"
#include "esp_mac.h"
#include "relinow_espnow.h"

static const char *TAG = "RELINOW_DEMO";
static relinow_espnow_node_t g_node;

static void on_message(const uint8_t src_mac[6], uint8_t channel_id, uint16_t seq_id, const uint8_t* payload, uint16_t payload_len, void* user_ctx) {
    ESP_LOGI(TAG, "RECV from %02X:%02X:%02X:%02X:%02X:%02X, channel %d, seq %d, len %d", 
             src_mac[0], src_mac[1], src_mac[2], src_mac[3], src_mac[4], src_mac[5], channel_id, seq_id, payload_len);
}

static void on_peer_timeout(const uint8_t peer_mac[6], void* user_ctx) {
    ESP_LOGW(TAG, "PEER TIMEOUT ! %02X:%02X:%02X:%02X:%02X:%02X is dead.",
             peer_mac[0], peer_mac[1], peer_mac[2], peer_mac[3], peer_mac[4], peer_mac[5]);
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_now_init());

    relinow_espnow_config_t rn_cfg;
    relinow_espnow_default_config(&rn_cfg);
    rn_cfg.heartbeat_interval_ms = 2000;
    rn_cfg.heartbeat_miss_count_max = 3;
    rn_cfg.on_message = on_message;
    rn_cfg.on_peer_timeout = on_peer_timeout;
    
    uint8_t peer_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    memcpy(rn_cfg.peer_mac, peer_mac, 6);
    
    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, peer_mac, 6);
    peer_info.channel = 0;
    peer_info.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));

    ESP_ERROR_CHECK(relinow_espnow_init(&g_node, &rn_cfg));
    
    ESP_LOGI(TAG, "=== DEMO SCENARIO START ===");
    
    // 1. Phase UNRELIABLE
    ESP_LOGI(TAG, "Phase 1: UNRELIABLE burst");
    relinow_espnow_open_channel(&g_node, 10, RELINOW_MODE_UNRELIABLE, 10);
    for (int i=0; i<5; i++) {
        relinow_espnow_send_unreliable(&g_node, 10, (uint8_t*)"RawData", 7);
    }
    
    // 2. Phase RELIABLE
    ESP_LOGI(TAG, "Phase 2: RELIABLE critical message");
    relinow_espnow_open_channel(&g_node, 20, RELINOW_MODE_RELIABLE, 10);
    relinow_espnow_send_reliable(&g_node, 20, (uint8_t*)"Hello Wokwi", 11, xTaskGetTickCount() * portTICK_PERIOD_MS);
    
    // 3. Phase MULTIPLEX
    ESP_LOGI(TAG, "Phase 3: MULTIPLEXING multiple reliable channels");
    relinow_espnow_open_channel(&g_node, 21, RELINOW_MODE_RELIABLE, 10);
    relinow_espnow_open_channel(&g_node, 22, RELINOW_MODE_RELIABLE, 10);
    relinow_espnow_send_reliable(&g_node, 21, (uint8_t*)"Chan 21 msg", 11, xTaskGetTickCount() * portTICK_PERIOD_MS);
    relinow_espnow_send_reliable(&g_node, 22, (uint8_t*)"Chan 22 msg", 11, xTaskGetTickCount() * portTICK_PERIOD_MS);

    // 4. Phase HEARTBEAT (Wait for timeout)
    ESP_LOGI(TAG, "Phase 4: Waiting for heartbeat timeout...");
    while (1) {
        relinow_espnow_poll(&g_node, xTaskGetTickCount() * portTICK_PERIOD_MS);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

