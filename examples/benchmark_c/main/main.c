#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "relinow_espnow.h"

static const char* TAG = "BENCHMARK";

#define BENCHMARK_CHANNEL 10
#define PACKET_COUNT 1000
#define PAYLOAD_SIZE 200

static relinow_espnow_node_t relinow_node;
static uint8_t my_mac[6];
static uint8_t peer_mac[6];
static bool peer_discovered = false;
static bool am_i_sender = false;
static uint8_t bcast_idx = 0;

static void discover_task(void* arg) {
    uint8_t bcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    
    esp_now_peer_info_t peer_info = {0};
    memcpy(peer_info.peer_addr, bcast_mac, 6);
    peer_info.channel = 1;
    peer_info.ifidx = WIFI_IF_STA;
    esp_now_add_peer(&peer_info);

    relinow_state_add_peer(&relinow_node.state, bcast_mac, &bcast_idx);
    relinow_state_open_channel(&relinow_node.state, bcast_idx, 1, RELINOW_MODE_UNRELIABLE, 5);
    relinow_node.peer_index = bcast_idx; // Temporarily point to broadcast

    ESP_LOGI(TAG, "Starting Auto-Discovery Broadcast...");
    
    while (!peer_discovered) {
        // Broadcast a ping
        relinow_espnow_send_unreliable(&relinow_node, 1, (const uint8_t*)"DISCOVER", 8);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    vTaskDelete(NULL);
}

static void app_recv_cb(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
    if (!peer_discovered) {
        if (memcmp(esp_now_info->src_addr, "\xFF\xFF\xFF\xFF\xFF\xFF", 6) != 0) {
            memcpy(peer_mac, esp_now_info->src_addr, 6);
            peer_discovered = true;
            
            // Determine role: Lowest MAC address is Sender
            am_i_sender = (memcmp(my_mac, peer_mac, 6) < 0);
            
            ESP_LOGW(TAG, "Peer Discovered: %02X:%02X:%02X:%02X:%02X:%02X", 
                peer_mac[0], peer_mac[1], peer_mac[2], peer_mac[3], peer_mac[4], peer_mac[5]);
            ESP_LOGW(TAG, "My Role: %s", am_i_sender ? "SENDER (Initiator)" : "RECEIVER (Responder)");
        }
    }
    
    relinow_espnow_on_recv(&relinow_node, esp_now_info->src_addr, data, data_len);
}

static void benchmark_task(void* arg) {
    while (!peer_discovered) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Peer found! Register it.
    esp_now_peer_info_t peer_info = {0};
    memcpy(peer_info.peer_addr, peer_mac, 6);
    peer_info.channel = 1;
    peer_info.ifidx = WIFI_IF_STA;
    esp_now_add_peer(&peer_info);

    uint8_t p_idx;
    relinow_state_add_peer(&relinow_node.state, peer_mac, &p_idx);
    relinow_node.peer_index = p_idx; // Switch to the real peer
    
    relinow_reliable_config_t cfg;
    relinow_reliable_default_config(&cfg);
    cfg.initial_rtt_ms = 50; 
    relinow_state_set_reliable_config(&relinow_node.state, p_idx, BENCHMARK_CHANNEL, &cfg);
    relinow_state_open_channel(&relinow_node.state, p_idx, BENCHMARK_CHANNEL, RELINOW_MODE_RELIABLE, 5);
    
    ESP_LOGI(TAG, "Waiting 3 seconds to synchronize...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    if (am_i_sender) {
        ESP_LOGI(TAG, "--- STARTING BENCHMARK ---");
        uint8_t payload[PAYLOAD_SIZE];
        memset(payload, 0xAB, PAYLOAD_SIZE);
        
        int packets_sent = 0;
        
        uint64_t start_time = esp_timer_get_time();
        
        while (1) {
            uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
            
            relinow_espnow_poll(&relinow_node, now);
            
            if (packets_sent < PACKET_COUNT) {
                esp_err_t err = relinow_espnow_send_reliable(&relinow_node, BENCHMARK_CHANNEL, payload, PAYLOAD_SIZE, now);
                if (err == ESP_OK) {
                    packets_sent++;
                    if (packets_sent % 100 == 0) {
                        ESP_LOGI(TAG, "Sent %d/%d...", packets_sent, PACKET_COUNT);
                    }
                }
            }
            
            uint16_t next_tx = relinow_node.state.peers[p_idx].channels[BENCHMARK_CHANNEL].next_tx_seq;
            uint8_t inflight = relinow_node.state.peers[p_idx].channels[BENCHMARK_CHANNEL].has_inflight;
            
            if (next_tx == PACKET_COUNT && !inflight) {
                break;
            }
            
            vTaskDelay(1);
        }
        
        uint64_t end_time = esp_timer_get_time();
        float duration_s = (end_time - start_time) / 1000000.0f;
        float total_bytes = PACKET_COUNT * PAYLOAD_SIZE;
        float kbps = (total_bytes / 1024.0f) / duration_s;
        
        uint16_t rtt = 0;
        relinow_state_reliable_get_rtt_ms(&relinow_node.state, p_idx, BENCHMARK_CHANNEL, &rtt);
        
        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, " BENCHMARK COMPLETE");
        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, "Packets: %d", PACKET_COUNT);
        ESP_LOGI(TAG, "Payload: %d bytes/packet", PAYLOAD_SIZE);
        ESP_LOGI(TAG, "Time   : %.2f seconds", duration_s);
        ESP_LOGI(TAG, "Speed  : %.2f KB/s (%.2f Mbps)", kbps, (kbps * 8.0f) / 1024.0f);
        ESP_LOGI(TAG, "Avg RTT: %d ms", rtt);
        ESP_LOGI(TAG, "========================================");

        while(1) { vTaskDelay(1000); }
    } else {
        ESP_LOGI(TAG, "--- RESPONDER READY ---");
        while (1) {
            uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
            relinow_espnow_poll(&relinow_node, now);
            vTaskDelay(1);
        }
    }
}

void app_main(void) {
    nvs_flash_init();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    esp_read_mac(my_mac, ESP_MAC_WIFI_STA);
    ESP_LOGW(TAG, "My MAC Address: %02X:%02X:%02X:%02X:%02X:%02X",
             my_mac[0], my_mac[1], my_mac[2], my_mac[3], my_mac[4], my_mac[5]);

    esp_now_init();
    esp_now_register_recv_cb(app_recv_cb);

    relinow_espnow_init(&relinow_node, 250);

    xTaskCreate(discover_task, "discover", 4096, NULL, 5, NULL);
    xTaskCreate(benchmark_task, "benchmark", 4096, NULL, 5, NULL);
}
