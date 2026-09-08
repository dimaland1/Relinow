#ifndef RELINOW_ESPNOW_H
#define RELINOW_ESPNOW_H

#include <stdint.h>

#include "esp_err.h"
#include "esp_now.h"

#include "relinow_packet.h"
#include "relinow_reliable.h"
#include "relinow_state.h"

#define RELINOW_ESPNOW_MAX_PAYLOAD 241u
#define RELINOW_ESPNOW_MAX_FRAME (RELINOW_HEADER_SIZE + RELINOW_ESPNOW_MAX_PAYLOAD)
#define RELINOW_ESPNOW_MAX_REASSEMBLY (RELINOW_ESPNOW_MAX_PAYLOAD * RELINOW_RELIABLE_MAX_PENDING)

typedef void (*relinow_espnow_on_message_cb)(
    const uint8_t src_mac[6],
    uint8_t channel_id,
    uint16_t seq_id,
    const uint8_t* payload,
    uint16_t payload_len,
    void* user_ctx
);

typedef void (*relinow_espnow_on_tx_event_cb)(
    relinow_reliable_tx_event_t event,
    uint16_t seq_id,
    void* user_ctx
);

typedef void (*relinow_espnow_on_peer_timeout_cb)(
    const uint8_t peer_mac[6],
    void* user_ctx
);

typedef struct {
    uint8_t peer_mac[6];
    uint8_t channel_id;
    uint8_t mode;
    uint16_t max_payload;
    uint16_t fragment_timeout_ms;
    uint16_t heartbeat_interval_ms;
    uint8_t heartbeat_miss_count_max;
    relinow_reliable_config_t reliable_cfg;
    relinow_espnow_on_message_cb on_message;
    relinow_espnow_on_tx_event_cb on_tx_event;
    relinow_espnow_on_peer_timeout_cb on_peer_timeout;
    void* user_ctx;
} relinow_espnow_config_t;

typedef struct {
    uint8_t in_use;
    uint16_t seq_id;
    uint8_t flags;
    uint16_t payload_len;
    uint8_t payload[RELINOW_ESPNOW_MAX_PAYLOAD];
} relinow_espnow_tx_cache_t;

typedef struct {
    uint8_t in_use;
    uint16_t seq_id;
    uint8_t flags;
    uint16_t payload_len;
    uint8_t payload[RELINOW_ESPNOW_MAX_PAYLOAD];
} relinow_espnow_rx_cache_t;

typedef struct {
    relinow_state_t state;
    uint8_t peer_index;
    uint8_t peer_mac[6];
    uint8_t channel_id;
    uint16_t max_payload;
    uint16_t fragment_timeout_ms;
    relinow_espnow_on_message_cb on_message;
    relinow_espnow_on_tx_event_cb on_tx_event;
    relinow_espnow_on_peer_timeout_cb on_peer_timeout;
    void* user_ctx;
    uint8_t fragmented_tx_active;
    uint8_t fragmented_tx_count;
    uint16_t fragmented_tx_seq[RELINOW_RELIABLE_MAX_PENDING];
    uint8_t reassembly_active;
    uint16_t reassembly_first_seq;
    uint16_t reassembly_len;
    uint32_t reassembly_start_ms;
    uint8_t reassembly_buf[RELINOW_ESPNOW_MAX_REASSEMBLY];
    relinow_espnow_tx_cache_t tx_cache[RELINOW_RELIABLE_MAX_PENDING];
    relinow_espnow_rx_cache_t rx_cache[1u + RELINOW_RELIABLE_MAX_REORDER];
} relinow_espnow_node_t;

void relinow_espnow_default_config(relinow_espnow_config_t* out_cfg);

esp_err_t relinow_espnow_init(
    relinow_espnow_node_t* node,
    const relinow_espnow_config_t* cfg
);

esp_err_t relinow_espnow_open_channel(
    relinow_espnow_node_t* node,
    uint8_t channel_id,
    uint8_t mode,
    uint8_t priority
);

esp_err_t relinow_espnow_send_reliable(
    relinow_espnow_node_t* node,
    const uint8_t* payload,
    uint16_t payload_len,
    uint32_t now_ms
);

esp_err_t relinow_espnow_send_unreliable(
    relinow_espnow_node_t* node,
    uint8_t channel_id,
    const uint8_t* payload,
    uint16_t payload_len
);

esp_err_t relinow_espnow_send_priority(
    relinow_espnow_node_t* node,
    uint8_t channel_id,
    const uint8_t* payload,
    uint16_t payload_len
);

esp_err_t relinow_espnow_on_receive(
    relinow_espnow_node_t* node,
    const uint8_t src_mac[6],
    const uint8_t* data,
    uint16_t data_len,
    uint32_t now_ms
);

esp_err_t relinow_espnow_poll(
    relinow_espnow_node_t* node,
    uint32_t now_ms
);

void relinow_espnow_on_send_status(
    relinow_espnow_node_t* node,
    const uint8_t dst_mac[6],
    esp_now_send_status_t status
);

#endif
