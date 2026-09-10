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

static relinow_channel_state_t* relinow_find_channel_state(relinow_espnow_node_t* node, uint8_t channel_id) {
    relinow_peer_state_t* peer;
    uint8_t i;

    if (node == 0 || node->peer_index >= RELINOW_MAX_PEERS) {
        return 0;
    }

    peer = &node->state.peers[node->peer_index];
    if (!peer->in_use) {
        return 0;
    }

    for (i = 0u; i < RELINOW_MAX_CHANNELS_PER_PEER; ++i) {
        relinow_channel_state_t* channel = &peer->channels[i];
        if (channel->in_use && channel->channel_id == channel_id) {
            return channel;
        }
    }

    return 0;
}

static uint8_t relinow_count_free_pending(const relinow_channel_state_t* channel) {
    uint8_t i;
    uint8_t free_count = 0u;

    if (channel == 0) {
        return 0u;
    }

    for (i = 0u; i < RELINOW_RELIABLE_MAX_PENDING; ++i) {
        if (!channel->reliable.pending[i].in_use) {
            ++free_count;
        }
    }

    return free_count;
}

static uint8_t relinow_count_free_tx_cache(const relinow_espnow_node_t* node) {
    uint8_t i;
    uint8_t free_count = 0u;

    if (node == 0) {
        return 0u;
    }

    for (i = 0u; i < RELINOW_RELIABLE_MAX_PENDING; ++i) {
        if (!node->tx_cache[i].in_use) {
            ++free_count;
        }
    }

    return free_count;
}

static void relinow_fragment_tx_track(relinow_espnow_node_t* node, uint16_t seq_id) {
    if (node == 0) {
        return;
    }
    if (node->fragmented_tx_count < RELINOW_RELIABLE_MAX_PENDING) {
        node->fragmented_tx_seq[node->fragmented_tx_count++] = seq_id;
        node->fragmented_tx_active = 1u;
    }
}

static void relinow_fragment_tx_on_ack(relinow_espnow_node_t* node, uint16_t ack_id) {
    uint8_t i;

    if (node == 0 || !node->fragmented_tx_active) {
        return;
    }

    for (i = 0u; i < node->fragmented_tx_count; ++i) {
        if (node->fragmented_tx_seq[i] == ack_id) {
            uint8_t j;
            for (j = i + 1u; j < node->fragmented_tx_count; ++j) {
                node->fragmented_tx_seq[j - 1u] = node->fragmented_tx_seq[j];
            }
            --node->fragmented_tx_count;
            if (node->fragmented_tx_count == 0u) {
                node->fragmented_tx_active = 0u;
            }
            return;
        }
    }
}

static void relinow_reassembly_reset(relinow_espnow_node_t* node) {
    if (node == 0) {
        return;
    }
    node->reassembly_active = 0u;
    node->reassembly_first_seq = 0u;
    node->reassembly_len = 0u;
    node->reassembly_start_ms = 0u;
}

static esp_err_t relinow_send_frame(
    relinow_espnow_node_t* node,
    uint8_t mode,
    uint8_t channel_id,
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
    header.mode = mode;
    header.type = type;
    header.flags = flags;
    header.seq_id = seq_id;
    header.ack_id = ack_id;
    header.channel_id = channel_id;
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
    out_cfg->mode = RELINOW_MODE_RELIABLE;
    out_cfg->channel_id = 1u;
    out_cfg->max_payload = RELINOW_ESPNOW_MAX_PAYLOAD;
    out_cfg->fragment_timeout_ms = 5000u;
    out_cfg->heartbeat_interval_ms = 0u;
    out_cfg->heartbeat_miss_count_max = 3u;
    out_cfg->on_peer_timeout = 0;
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

    src = relinow_state_open_channel(&node->state, node->peer_index, cfg->channel_id, cfg->mode, 0u);
    if (src != RELINOW_STATE_OK) {
        return ESP_FAIL;
    }

    if (cfg->mode == RELINOW_MODE_RELIABLE) {
        src = relinow_state_set_reliable_config(&node->state, node->peer_index, cfg->channel_id, &cfg->reliable_cfg);
        if (src != RELINOW_STATE_OK) {
            return ESP_FAIL;
        }
    }

    src = relinow_state_configure_heartbeat(&node->state, node->peer_index, cfg->heartbeat_interval_ms, cfg->heartbeat_miss_count_max);
    if (src != RELINOW_STATE_OK) {
        return ESP_FAIL;
    }

    memcpy(node->peer_mac, cfg->peer_mac, sizeof(node->peer_mac));
    node->channel_id = cfg->channel_id;
    node->max_payload = cfg->max_payload;
    node->fragment_timeout_ms = cfg->fragment_timeout_ms;
    node->on_message = cfg->on_message;
    node->on_tx_event = cfg->on_tx_event;
    node->on_peer_timeout = cfg->on_peer_timeout;
    node->user_ctx = cfg->user_ctx;
    return ESP_OK;
}

esp_err_t relinow_espnow_open_channel(
    relinow_espnow_node_t* node,
    uint8_t channel_id,
    uint8_t mode,
    uint8_t priority
) {
    relinow_state_err_t src;

    if (node == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    src = relinow_state_open_channel(&node->state, node->peer_index, channel_id, mode, priority);
    if (src != RELINOW_STATE_OK) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t relinow_espnow_send_reliable(
    relinow_espnow_node_t* node,
    uint8_t channel_id,
    const uint8_t* payload,
    uint16_t payload_len,
    uint32_t now_ms
) {
    relinow_reliable_tx_result_t tx;
    relinow_state_err_t src;
    uint8_t channel_mode;
    relinow_channel_state_t* channel;
    relinow_espnow_tx_cache_t* slot;
    esp_err_t erc;

    if (node == 0 || payload == 0 || payload_len == 0u) {
        return ESP_ERR_INVALID_ARG;
    }

    src = relinow_state_get_channel_mode(&node->state, node->peer_index, channel_id, &channel_mode);
    if (src != RELINOW_STATE_OK || channel_mode != RELINOW_MODE_RELIABLE) {
        return ESP_ERR_INVALID_STATE;
    }

    channel = relinow_find_channel_state(node, channel_id);
    if (channel == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    if (node->fragmented_tx_active) {
        return ESP_ERR_INVALID_STATE;
    }

    if (payload_len > node->max_payload) {
        uint16_t remaining = payload_len;
        const uint8_t* cursor = payload;
        uint8_t fragment_count = (uint8_t)((payload_len + node->max_payload - 1u) / node->max_payload);

        if (fragment_count > RELINOW_RELIABLE_MAX_PENDING) {
            return ESP_ERR_NO_MEM;
        }
        if (relinow_count_free_pending(channel) < fragment_count || relinow_count_free_tx_cache(node) < fragment_count) {
            return ESP_ERR_NO_MEM;
        }

        node->fragmented_tx_active = 1u;
        node->fragmented_tx_count = 0u;

        while (remaining > 0u) {
            uint16_t frag_len = (remaining > node->max_payload) ? node->max_payload : remaining;
            uint8_t frag_flags = RELINOW_FLAG_FRAGMENT;

            if (remaining == frag_len) {
                frag_flags = (uint8_t)(frag_flags | RELINOW_FLAG_LAST_FRAGMENT);
            }

            src = relinow_state_reliable_send(&node->state, node->peer_index, channel_id, now_ms, &tx);
            if (src != RELINOW_STATE_OK || tx.event != RELINOW_RELIABLE_TX_NEW) {
                if (node->fragmented_tx_count == 0u) {
                    node->fragmented_tx_active = 0u;
                }
                return ESP_FAIL;
            }

            slot = relinow_tx_cache_alloc(node);
            if (slot == 0) {
                return ESP_ERR_NO_MEM;
            }

            memset(slot, 0, sizeof(*slot));
            slot->in_use = 1u;
            slot->seq_id = tx.seq_id;
            slot->flags = frag_flags;
            slot->payload_len = frag_len;
            memcpy(slot->payload, cursor, frag_len);

            relinow_fragment_tx_track(node, tx.seq_id);

            erc = relinow_send_frame(node, RELINOW_MODE_RELIABLE, channel_id, RELINOW_TYPE_DATA, slot->flags, tx.seq_id, 0u, slot->payload, slot->payload_len);
            if (erc != ESP_OK) {
                return erc;
            }

            if (node->on_tx_event != 0) {
                node->on_tx_event(RELINOW_RELIABLE_TX_NEW, tx.seq_id, node->user_ctx);
            }

            cursor += frag_len;
            remaining = (uint16_t)(remaining - frag_len);
        }

        return ESP_OK;
    }

    src = relinow_state_reliable_send(&node->state, node->peer_index, channel_id, now_ms, &tx);
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
    slot->flags = 0u;
    slot->payload_len = payload_len;
    memcpy(slot->payload, payload, payload_len);

    erc = relinow_send_frame(node, RELINOW_MODE_RELIABLE, channel_id, RELINOW_TYPE_DATA, slot->flags, tx.seq_id, 0u, slot->payload, slot->payload_len);
    if (erc != ESP_OK) {
        return erc;
    }

    if (node->on_tx_event != 0) {
        node->on_tx_event(RELINOW_RELIABLE_TX_NEW, tx.seq_id, node->user_ctx);
    }

    return ESP_OK;
}

esp_err_t relinow_espnow_send_unreliable(
    relinow_espnow_node_t* node,
    uint8_t channel_id,
    const uint8_t* payload,
    uint16_t payload_len
) {
    relinow_state_err_t src;
    uint8_t channel_mode;
    uint16_t seq_id;

    if (node == 0 || payload == 0 || payload_len == 0u || payload_len > node->max_payload) {
        return ESP_ERR_INVALID_ARG;
    }

    src = relinow_state_get_channel_mode(&node->state, node->peer_index, channel_id, &channel_mode);
    if (src != RELINOW_STATE_OK || channel_mode != RELINOW_MODE_UNRELIABLE) {
        return ESP_ERR_INVALID_STATE;
    }

    src = relinow_state_unreliable_send(&node->state, node->peer_index, channel_id, &seq_id);
    if (src != RELINOW_STATE_OK) {
        return ESP_FAIL;
    }

    return relinow_send_frame(node, RELINOW_MODE_UNRELIABLE, channel_id, RELINOW_TYPE_DATA, 0u, seq_id, 0u, payload, payload_len);
}

esp_err_t relinow_espnow_send_priority(
    relinow_espnow_node_t* node,
    uint8_t channel_id,
    const uint8_t* payload,
    uint16_t payload_len
) {
    relinow_state_err_t src;
    uint8_t channel_mode;
    uint16_t seq_id;
    uint8_t replaced;
    uint16_t replaced_seq_id;

    if (node == 0 || payload == 0 || payload_len == 0u || payload_len > node->max_payload) {
        return ESP_ERR_INVALID_ARG;
    }

    src = relinow_state_get_channel_mode(&node->state, node->peer_index, channel_id, &channel_mode);
    if (src != RELINOW_STATE_OK || channel_mode != RELINOW_MODE_PRIORITY) {
        return ESP_ERR_INVALID_STATE;
    }

    src = relinow_state_priority_send(
        &node->state,
        node->peer_index,
        channel_id,
        &seq_id,
        &replaced,
        &replaced_seq_id
    );
    if (src != RELINOW_STATE_OK) {
        return ESP_FAIL;
    }

    (void)replaced;
    (void)replaced_seq_id;
    return relinow_send_frame(node, RELINOW_MODE_PRIORITY, channel_id, RELINOW_TYPE_DATA, 0u, seq_id, 0u, payload, payload_len);
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
    relinow_state_err_t src;
    uint8_t channel_mode = 0u;

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

    if (header.channel_id == RELINOW_HEARTBEAT_CHANNEL) {
        if (header.type == RELINOW_TYPE_PING) {
            (void)relinow_send_frame(node, header.mode, RELINOW_HEARTBEAT_CHANNEL, RELINOW_TYPE_PONG, 0u, 0u, header.seq_id, 0, 0u);
        } else if (header.type == RELINOW_TYPE_PONG) {
            (void)relinow_state_on_pong(&node->state, node->peer_index);
        }
        return ESP_OK;
    }

    src = relinow_state_get_channel_mode(&node->state, node->peer_index, header.channel_id, &channel_mode);
    if (src != RELINOW_STATE_OK || channel_mode != header.mode) {
        return ESP_OK;
    }

    if (header.mode == RELINOW_MODE_UNRELIABLE) {
        if (header.type == RELINOW_TYPE_DATA) {
            (void)relinow_state_unreliable_on_data(&node->state, node->peer_index, header.channel_id, header.seq_id);
            if (node->on_message != 0) {
                node->on_message(src_mac, header.channel_id, header.seq_id, &data[RELINOW_HEADER_SIZE], header.payload_len, node->user_ctx);
            }
        }
        return ESP_OK;
    }

    if (header.mode == RELINOW_MODE_PRIORITY) {
        if (header.type == RELINOW_TYPE_DATA) {
            uint8_t should_deliver = 0u;
            if (relinow_state_priority_on_data(&node->state, node->peer_index, header.channel_id, header.seq_id, &should_deliver) != RELINOW_STATE_OK) {
                return ESP_FAIL;
            }
            if (should_deliver && node->on_message != 0) {
                node->on_message(src_mac, header.channel_id, header.seq_id, &data[RELINOW_HEADER_SIZE], header.payload_len, node->user_ctx);
            }
        }
        return ESP_OK;
    }

    if (header.mode != RELINOW_MODE_RELIABLE) {
        return ESP_OK;
    }

    if (header.type == RELINOW_TYPE_ACK) {
        if (relinow_state_reliable_on_ack(&node->state, node->peer_index, header.channel_id, header.ack_id, now_ms) == RELINOW_STATE_OK) {
            relinow_tx_cache_remove(node, header.ack_id);
            relinow_fragment_tx_on_ack(node, header.ack_id);
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
                rx_slot->flags = header.flags;
                rx_slot->payload_len = header.payload_len;
                if (header.payload_len > 0u) {
                    memcpy(rx_slot->payload, &data[RELINOW_HEADER_SIZE], header.payload_len);
                }
            }
        }

        if (relinow_state_reliable_on_data(&node->state, node->peer_index, header.channel_id, header.seq_id, &rx) != RELINOW_STATE_OK) {
            return ESP_FAIL;
        }

        (void)relinow_send_frame(node, RELINOW_MODE_RELIABLE, header.channel_id, RELINOW_TYPE_ACK, 0u, 0u, rx.ack_id, 0, 0u);

        for (i = 0u; i < rx.delivered_count; ++i) {
            relinow_espnow_rx_cache_t* delivered = relinow_rx_cache_find(node, rx.delivered_seq[i]);
            if (delivered != 0u) {
                if ((delivered->flags & RELINOW_FLAG_FRAGMENT) != 0u) {
                    uint32_t combined_len;

                    if (!node->reassembly_active) {
                        node->reassembly_active = 1u;
                        node->reassembly_first_seq = delivered->seq_id;
                        node->reassembly_len = 0u;
                        node->reassembly_start_ms = now_ms;
                    }

                    combined_len = (uint32_t)node->reassembly_len + delivered->payload_len;
                    if (combined_len > RELINOW_ESPNOW_MAX_REASSEMBLY) {
                        relinow_reassembly_reset(node);
                    } else {
                        if (delivered->payload_len > 0u) {
                            memcpy(&node->reassembly_buf[node->reassembly_len], delivered->payload, delivered->payload_len);
                            node->reassembly_len = (uint16_t)combined_len;
                        }

                        if ((delivered->flags & RELINOW_FLAG_LAST_FRAGMENT) != 0u) {
                            if (node->on_message != 0) {
                                node->on_message(src_mac, header.channel_id, node->reassembly_first_seq, node->reassembly_buf, node->reassembly_len, node->user_ctx);
                            }
                            relinow_reassembly_reset(node);
                        }
                    }
                } else {
                    if (node->reassembly_active) {
                        relinow_reassembly_reset(node);
                    }
                    if (node->on_message != 0) {
                        node->on_message(src_mac, header.channel_id, delivered->seq_id, delivered->payload, delivered->payload_len, node->user_ctx);
                    }
                }
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

    if (node->reassembly_active && (now_ms - node->reassembly_start_ms > node->fragment_timeout_ms)) {
        relinow_reassembly_reset(node);
    }

    {
        uint8_t should_ping = 0;
        uint8_t peer_timeout = 0;
        relinow_state_poll_heartbeat(&node->state, node->peer_index, now_ms, &should_ping, &peer_timeout);
        if (peer_timeout && node->on_peer_timeout) {
            node->on_peer_timeout(node->peer_mac, node->user_ctx);
        } else if (should_ping) {
            (void)relinow_send_frame(node, RELINOW_MODE_UNRELIABLE, RELINOW_HEARTBEAT_CHANNEL, RELINOW_TYPE_PING, 0u, 0u, 0u, 0, 0u);
        }
    }

    {
        uint8_t sched_channel = 0;
        src = relinow_state_scheduler_next(&node->state, node->peer_index, now_ms, &sched_channel, &tx);
        if (src == RELINOW_STATE_OK && tx.event != RELINOW_RELIABLE_TX_NONE) {
            if (tx.event == RELINOW_RELIABLE_TX_RETRANSMIT) {
                relinow_espnow_tx_cache_t* slot = relinow_tx_cache_find(node, tx.seq_id);
                if (slot != 0u) {
                    esp_err_t erc = relinow_send_frame(node, RELINOW_MODE_RELIABLE, sched_channel, RELINOW_TYPE_DATA, slot->flags, slot->seq_id, 0u, slot->payload, slot->payload_len);
                    if (erc != ESP_OK) {
                        return erc;
                    }
                }
            } else if (tx.event == RELINOW_RELIABLE_TX_FAILED) {
                relinow_tx_cache_remove(node, tx.seq_id);
            }
            if (node->on_tx_event != 0) {
                node->on_tx_event(tx.event, tx.seq_id, node->user_ctx);
            }
        }
    }

    return ESP_OK;
}

void relinow_espnow_on_send_status(
    relinow_espnow_node_t* node,
    const uint8_t dst_mac[6],
    esp_now_send_status_t status
) {
    (void)status;

    if (node == 0 || dst_mac == 0) {
        return;
    }
    if (memcmp(dst_mac, node->peer_mac, 6u) != 0) {
        return;
    }
}
