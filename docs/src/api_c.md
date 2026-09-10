# C99 Core & ESP-IDF Adapter API

The C implementation of ReliNow is structured into two layers:
1. **Core Engine (`core/include/relinow_state.h`)**: Pure C99, zero-dependency, zero dynamic allocation (`malloc`). Suitable for any microcontroller.
2. **Transport Adapter (`core/include/relinow_espnow.h`)**: Bridges the core protocol with the ESP-IDF Wi-Fi/ESP-NOW stack and FreeRTOS.

---

## 1. Core Header Files

| Header | Role |
| :--- | :--- |
| `relinow_packet.h` | 9-byte packet format serialization, deserialization, endianness handling, and validation. |
| `relinow_state.h` | Peer table, channel state machines, sequence tracking, and channel scheduler. |
| `relinow_reliable.h` | Sliding window queue, selective ACK evaluation, dynamic RTT estimation (Jacobson's algorithm), and timeout backoff. |
| `relinow_espnow.h` | High-level adapter handling ESP-NOW callbacks, packet fragmentation, and background polling. |

---

## 2. Key Data Structures

### Configuration: `relinow_config_t`
```c
typedef struct {
    uint8_t peer_mac[6];              // Target peer MAC address (or broadcast FF:FF:FF:FF:FF:FF)
    uint8_t channel_id;               // Primary channel ID (0-15)
    uint8_t mode;                     // RELINOW_MODE_RELIABLE, UNRELIABLE, or PRIORITY
    uint16_t max_payload;             // Maximum payload per frame (up to 241 bytes)
    uint32_t fragment_timeout_ms;     // Reassembly timeout for multi-frame packets (e.g., 5000ms)
    uint32_t heartbeat_interval_ms;   // PING interval on channel 0xFF (e.g., 1000ms)
    uint8_t heartbeat_miss_count_max; // Missed PONG threshold before triggering timeout
    relinow_reliable_config_t reliable_cfg; // Initial RTT and window settings
    relinow_on_message_cb on_message;       // Callback when a complete message is received
    relinow_on_tx_event_cb on_tx_event;     // Callback on transmission events (ACK, drop)
    relinow_on_peer_timeout_cb on_peer_timeout; // Callback when a peer stops responding
    void* user_ctx;                   // User context pointer passed to callbacks
} relinow_config_t;
```

---

## 3. Core Lifecycle Functions

### `relinow_init`
```c
esp_err_t relinow_init(relinow_node_t* node, const relinow_config_t* cfg);
```
Initializes the node state, zeroing internal buffers, setting up default channels, and preparing transmission queues.

### `relinow_send_reliable`
```c
esp_err_t relinow_send_reliable(
    relinow_node_t* node,
    uint8_t channel_id,
    const uint8_t* payload,
    uint16_t payload_len,
    uint32_t now_ms
);
```
Enqueues a payload for guaranteed delivery. If `payload_len > node->max_payload`, the message is automatically fragmented across multiple sequence-numbered frames.

### `relinow_on_receive`
```c
esp_err_t relinow_on_receive(
    relinow_node_t* node,
    const uint8_t src_mac[6],
    const uint8_t* data,
    uint16_t data_len,
    uint32_t now_ms
);
```
Must be invoked from ESP-NOW's native `esp_now_register_recv_cb`. Parses incoming headers, manages ACK generation, feeds the reassembly engine, and triggers `on_message` once complete data is available.

### `relinow_poll`
```c
esp_err_t relinow_poll(relinow_node_t* node, uint32_t now_ms);
```
Drives the internal timers, evaluates retransmission timeouts for unacknowledged frames, handles Heartbeat PING emissions, and reclaims expired fragment buffers. Should be called periodically (e.g. every 1-10 ms) in a background task.

---

## 4. Minimal ESP-IDF Working Example

```c
#include "esp_wifi.h"
#include "esp_now.h"
#include "relinow_espnow.h"

static relinow_node_t g_node;

static void on_message(const uint8_t* src_mac, uint8_t channel_id, uint16_t seq_id,
                       const uint8_t* payload, uint16_t len, void* ctx) {
    printf("Received %d bytes on channel %d: %.*s\n", len, channel_id, len, payload);
}

static void espnow_recv_cb(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    relinow_on_receive(&g_node, info->src_addr, data, len, now);
}

void app_main(void) {
    // 1. Initialize Wi-Fi & ESP-NOW
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wcfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_now_init();
    esp_now_register_recv_cb(espnow_recv_cb);

    // 2. Configure ReliNow
    relinow_config_t cfg;
    relinow_default_config(&cfg);
    cfg.channel_id = 10;
    cfg.mode = RELINOW_MODE_RELIABLE;
    cfg.on_message = on_message;
    relinow_init(&g_node, &cfg);

    // 3. Main transmission and polling loop
    while (1) {
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        relinow_poll(&g_node, now);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
```
