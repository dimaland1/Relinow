#include "relinow_espnow.h"

#include <string.h>

static relinow_espnow_tx_cache_t* relinow_tx_cache_find(relinow_espnow_node_t* node, uint16_t seq_id) {
    uint8_t i;
    for (i = 0u; i < RELINOW_RELIABLE_MAX_PENDING; ++i) {
        if (node->tx_cache[i].in_use && node->tx_cache[i].seq_id == seq_id) {
            return &node->tx_cache[i];
        }
    }
    return 0;
}

static relinow_espnow_tx_cache_t* relinow_tx_cache_alloc(relinow_espnow_node_t* node) {
    uint8_t i;
    for (i = 0u; i < RELINOW_RELIABLE_MAX_PENDING; ++i) {
        if (!node->tx_cache[i].in_use) {
            return &node->tx_cache[i];
        }
    }
    return 0;
}

static void relinow_tx_cache_remove(relinow_espnow_node_t* node, uint16_t seq_id) {
    relinow_espnow_tx_cache_t* slot = relinow_tx_cache_find(node, seq_id);
    if (slot != 0) {
        slot->in_use = 0u;
    }
}

static relinow_espnow_rx_cache_t* relinow_rx_cache_find(relinow_espnow_node_t* node, uint16_t seq_id) {
    uint8_t i;
    for (i = 0u; i < (1u + RELINOW_RELIABLE_MAX_REORDER); ++i) {
        if (node->rx_cache[i].in_use && node->rx_cache[i].seq_id == seq_id) {
            return &node->rx_cache[i];
        }
    }
    return 0;
}

static relinow_espnow_rx_cache_t* relinow_rx_cache_alloc(relinow_espnow_node_t* node) {
    uint8_t i;
    for (i = 0u; i < (1u + RELINOW_RELIABLE_MAX_REORDER); ++i) {
        if (!node->rx_cache[i].in_use) {
            return &node->rx_cache[i];
        }
    }
    return 0;
}

static void relinow_rx_cache_remove(relinow_espnow_node_t* node, uint16_t seq_id) {
    relinow_espnow_rx_cache_t* slot = relinow_rx_cache_find(node, seq_id);
    if (slot != 0) {
        slot->in_use = 0u;
    }
}

static esp_err_t relinow_send_frame(
    relinow_espnow_node_t* node,
    uint8_t type,
    uint8_t flags,
    uint16_t seq_id,
    uint16_t ack_id,
    const uint8_t* payload,
    uint16_t payload_len
) {
    relinow_header_t header;
    uint8_t frame[RELINOW_ESPNOW_MAX_FRAME];
    relinow_err_t rc;

    if (payload_len > node->max_payload) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(&header, 0, sizeof(header));
    header.version = RELINOW_PROTOCOL_VERSION;
    header.mode = RELINOW_MODE_RELIABLE;
    header.type = type;
    header.flags = flags;
    header.seq_id = seq_id;
    header.ack_id = ack_id;
    header.channel_id = node->channel_id;
    header.payload_len = payload_len;

    rc = relinow_encode_header(&header, node->max_payload, frame);
    if (rc != RELINOW_ERR_OK) {
        return ESP_ERR_INVALID_ARG;
    }

    if (payload_len > 0u && payload != 0) {
        memcpy(&frame[RELINOW_HEADER_SIZE], payload, payload_len);
    }

    return esp_now_send(node->peer_mac, frame, (size_t)(RELINOW_HEADER_SIZE + payload_len));
}

void relinow_espnow_default_config(relinow_espnow_config_t* out_cfg) {
    relinow_reliable_config_t rel_cfg;
    if (out_cfg == 0) {
        return;
    }

    memset(out_cfg, 0, sizeof(*out_cfg));
    out_cfg->channel_id = 1u;
    out_cfg->max_payload = RELINOW_ESPNOW_MAX_PAYLOAD;
    relinow_reliable_default_config(&rel_cfg);
    out_cfg->reliable_cfg = rel_cfg;
}

esp_err_t relinow_espnow_init(
    relinow_espnow_node_t* node,
    const relinow_espnow_config_t* cfg
) {
    relinow_state_err_t src;
    if (node == 0 || cfg == 0 || cfg->max_payload == 0u || cfg->max_payload > RELINOW_ESPNOW_MAX_PAYLOAD) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(node, 0, sizeof(*node));
    relinow_state_init(&node->state);

    src = relinow_state_add_peer(&node->state, cfg->peer_mac, &node->peer_index);
    if (src != RELINOW_STATE_OK) {
        return ESP_FAIL;
    }

    src = relinow_state_open_channel(&node->state, node->peer_index, cfg->channel_id, RELINOW_MODE_RELIABLE, 0u);
    if (src != RELINOW_STATE_OK) {
        return ESP_FAIL;
    }

    src = relinow_state_set_reliable_config(&node->state, node->peer_index, cfg->channel_id, &cfg->reliable_cfg);
    if (src != RELINOW_STATE_OK) {
        return ESP_FAIL;
    }

    memcpy(node->peer_mac, cfg->peer_mac, sizeof(node->peer_mac));
    node->channel_id = cfg->channel_id;
    node->max_payload = cfg->max_payload;
    node->on_message = cfg->on_message;
    node->on_tx_event = cfg->on_tx_event;
    node->user_ctx = cfg->user_ctx;
    return ESP_OK;
}

esp_err_t relinow_espnow_send_reliable(
    relinow_espnow_node_t* node,
    const uint8_t* payload,
    uint16_t payload_len,
    uint32_t now_ms
) {
    relinow_reliable_tx_result_t tx;
    relinow_state_err_t src;
    relinow_espnow_tx_cache_t* slot;
    esp_err_t erc;

    if (node == 0 || payload == 0 || payload_len == 0u || payload_len > node->max_payload) {
        return ESP_ERR_INVALID_ARG;
    }

    src = relinow_state_reliable_send(&node->state, node->peer_index, node->channel_id, now_ms, &tx);
    if (src != RELINOW_STATE_OK || tx.event != RELINOW_RELIABLE_TX_NEW) {
        return ESP_FAIL;
    }

    slot = relinow_tx_cache_alloc(node);
    if (slot == 0) {
        return ESP_ERR_NO_MEM;
    }

    memset(slot, 0, sizeof(*slot));
    slot->in_use = 1u;
    slot->seq_id = tx.seq_id;
    slot->payload_len = payload_len;
    memcpy(slot->payload, payload, payload_len);

    erc = relinow_send_frame(node, RELINOW_TYPE_DATA, 0u, tx.seq_id, 0u, slot->payload, slot->payload_len);
    if (erc != ESP_OK) {
        return erc;
    }

    if (node->on_tx_event != 0) {
        node->on_tx_event(RELINOW_RELIABLE_TX_NEW, tx.seq_id, node->user_ctx);
    }

    return ESP_OK;
}

esp_err_t relinow_espnow_on_receive(
    relinow_espnow_node_t* node,
    const uint8_t src_mac[6],
    const uint8_t* data,
    uint16_t data_len,
    uint32_t now_ms
) {
    relinow_header_t header;
    relinow_err_t prc;

    if (node == 0 || src_mac == 0 || data == 0 || data_len < RELINOW_HEADER_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    if (memcmp(src_mac, node->peer_mac, 6u) != 0) {
        return ESP_OK;
    }

    prc = relinow_decode_header(data, data_len, node->max_payload, &header);
    if (prc != RELINOW_ERR_OK) {
        return ESP_OK;
    }

    if (header.mode != RELINOW_MODE_RELIABLE || header.channel_id != node->channel_id) {
        return ESP_OK;
    }

    if (header.type == RELINOW_TYPE_ACK) {
        if (relinow_state_reliable_on_ack(&node->state, node->peer_index, node->channel_id, header.ack_id, now_ms) == RELINOW_STATE_OK) {
            relinow_tx_cache_remove(node, header.ack_id);
        }
        return ESP_OK;
    }

    if (header.type == RELINOW_TYPE_DATA) {
        relinow_espnow_rx_cache_t* rx_slot;
        relinow_reliable_rx_result_t rx;
        uint8_t i;

        rx_slot = relinow_rx_cache_find(node, header.seq_id);
        if (rx_slot == 0u) {
            rx_slot = relinow_rx_cache_alloc(node);
            if (rx_slot != 0u) {
                rx_slot->in_use = 1u;
                rx_slot->seq_id = header.seq_id;
                rx_slot->payload_len = header.payload_len;
                if (header.payload_len > 0u) {
                    memcpy(rx_slot->payload, &data[RELINOW_HEADER_SIZE], header.payload_len);
                }
            }
        }

        if (relinow_state_reliable_on_data(&node->state, node->peer_index, node->channel_id, header.seq_id, &rx) != RELINOW_STATE_OK) {
            return ESP_FAIL;
        }

        (void)relinow_send_frame(node, RELINOW_TYPE_ACK, 0u, 0u, rx.ack_id, 0, 0u);

        for (i = 0u; i < rx.delivered_count; ++i) {
            relinow_espnow_rx_cache_t* delivered = relinow_rx_cache_find(node, rx.delivered_seq[i]);
            if (delivered != 0u && node->on_message != 0) {
                node->on_message(src_mac, node->channel_id, delivered->seq_id, delivered->payload, delivered->payload_len, node->user_ctx);
                relinow_rx_cache_remove(node, delivered->seq_id);
            }
        }

        return ESP_OK;
    }

    return ESP_OK;
}

esp_err_t relinow_espnow_poll(
    relinow_espnow_node_t* node,
    uint32_t now_ms
) {
    relinow_reliable_tx_result_t tx;
    relinow_state_err_t src;

    if (node == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    src = relinow_state_reliable_poll(&node->state, node->peer_index, node->channel_id, now_ms, &tx);
    if (src != RELINOW_STATE_OK) {
        return ESP_FAIL;
    }

    if (tx.event == RELINOW_RELIABLE_TX_RETRANSMIT) {
        relinow_espnow_tx_cache_t* slot = relinow_tx_cache_find(node, tx.seq_id);
        if (slot != 0u) {
            esp_err_t erc = relinow_send_frame(node, RELINOW_TYPE_DATA, 0u, slot->seq_id, 0u, slot->payload, slot->payload_len);
            if (erc != ESP_OK) {
                return erc;
            }
        }
    } else if (tx.event == RELINOW_RELIABLE_TX_FAILED) {
        relinow_tx_cache_remove(node, tx.seq_id);
    }

    if (tx.event != RELINOW_RELIABLE_TX_NONE && node->on_tx_event != 0) {
        node->on_tx_event(tx.event, tx.seq_id, node->user_ctx);
    }

    return ESP_OK;
}

void relinow_espnow_on_send_status(
    relinow_espnow_node_t* node,
    const uint8_t dst_mac[6],
    esp_now_send_status_t status
) {
    (void)node;
    (void)dst_mac;
    (void)status;
}
