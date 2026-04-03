#include "relinow_state.h"

#include <string.h>

static int relinow_mac_equal(const uint8_t* a, const uint8_t* b) {
    uint8_t i;
    for (i = 0; i < 6u; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static int relinow_mode_supported(uint8_t mode) {
    return mode == RELINOW_MODE_RELIABLE || mode == RELINOW_MODE_UNRELIABLE || mode == RELINOW_MODE_PRIORITY;
}

static relinow_channel_state_t* relinow_find_channel_mut(
    relinow_state_t* state,
    uint8_t peer_index,
    uint8_t channel_id
) {
    uint8_t i;
    relinow_peer_state_t* peer;
    if (state == 0 || peer_index >= RELINOW_MAX_PEERS) {
        return 0;
    }
    peer = &state->peers[peer_index];
    if (!peer->in_use) {
        return 0;
    }

    for (i = 0; i < RELINOW_MAX_CHANNELS_PER_PEER; ++i) {
        relinow_channel_state_t* channel = &peer->channels[i];
        if (channel->in_use && channel->channel_id == channel_id) {
            return channel;
        }
    }
    return 0;
}

void relinow_state_init(relinow_state_t* state) {
    if (state != 0) {
        memset(state, 0, sizeof(*state));
    }
}

void relinow_state_get_limits(relinow_state_limits_t* out_limits) {
    if (out_limits == 0) {
        return;
    }
    out_limits->max_peers = RELINOW_MAX_PEERS;
    out_limits->max_channels_per_peer = RELINOW_MAX_CHANNELS_PER_PEER;
    out_limits->tx_queue_size = RELINOW_TX_QUEUE_SIZE;
    out_limits->rx_queue_size = RELINOW_RX_QUEUE_SIZE;
    out_limits->state_footprint_bytes = sizeof(relinow_state_t);
}

relinow_state_err_t relinow_state_find_peer(
    const relinow_state_t* state,
    const uint8_t mac[6],
    uint8_t* out_peer_index
) {
    uint8_t i;
    if (state == 0 || mac == 0 || out_peer_index == 0) {
        return RELINOW_STATE_ERR_INVALID_ARG;
    }
    for (i = 0; i < RELINOW_MAX_PEERS; ++i) {
        if (state->peers[i].in_use && relinow_mac_equal(state->peers[i].mac, mac)) {
            *out_peer_index = i;
            return RELINOW_STATE_OK;
        }
    }
    return RELINOW_STATE_ERR_NOT_FOUND;
}

relinow_state_err_t relinow_state_add_peer(
    relinow_state_t* state,
    const uint8_t mac[6],
    uint8_t* out_peer_index
) {
    uint8_t i;
    uint8_t existing_idx;
    relinow_state_err_t existing_rc;

    if (state == 0 || mac == 0 || out_peer_index == 0) {
        return RELINOW_STATE_ERR_INVALID_ARG;
    }

    existing_rc = relinow_state_find_peer(state, mac, &existing_idx);
    if (existing_rc == RELINOW_STATE_OK) {
        *out_peer_index = existing_idx;
        return RELINOW_STATE_OK;
    }

    for (i = 0; i < RELINOW_MAX_PEERS; ++i) {
        if (!state->peers[i].in_use) {
            relinow_peer_state_t* peer = &state->peers[i];
            memset(peer, 0, sizeof(*peer));
            peer->in_use = 1u;
            memcpy(peer->mac, mac, 6u);
            *out_peer_index = i;
            return RELINOW_STATE_OK;
        }
    }
    return RELINOW_STATE_ERR_NO_SPACE;
}

relinow_state_err_t relinow_state_open_channel(
    relinow_state_t* state,
    uint8_t peer_index,
    uint8_t channel_id,
    uint8_t mode,
    uint8_t priority
) {
    uint8_t i;
    relinow_peer_state_t* peer;

    if (state == 0 || peer_index >= RELINOW_MAX_PEERS || !relinow_mode_supported(mode)) {
        return RELINOW_STATE_ERR_INVALID_ARG;
    }
    if (channel_id == RELINOW_HEARTBEAT_CHANNEL) {
        return RELINOW_STATE_ERR_INVALID_ARG;
    }

    peer = &state->peers[peer_index];
    if (!peer->in_use) {
        return RELINOW_STATE_ERR_NOT_FOUND;
    }

    for (i = 0; i < RELINOW_MAX_CHANNELS_PER_PEER; ++i) {
        relinow_channel_state_t* channel = &peer->channels[i];
        if (channel->in_use && channel->channel_id == channel_id) {
            if (channel->mode != mode) {
                return RELINOW_STATE_ERR_CONFLICT;
            }
            channel->priority = priority;
            return RELINOW_STATE_OK;
        }
    }

    for (i = 0; i < RELINOW_MAX_CHANNELS_PER_PEER; ++i) {
        relinow_channel_state_t* channel = &peer->channels[i];
        if (!channel->in_use) {
            memset(channel, 0, sizeof(*channel));
            channel->in_use = 1u;
            channel->channel_id = channel_id;
            channel->mode = mode;
            channel->priority = priority;
            channel->next_tx_seq = 0u;
            channel->expected_rx_seq = 0u;
            return RELINOW_STATE_OK;
        }
    }

    return RELINOW_STATE_ERR_NO_SPACE;
}

relinow_state_err_t relinow_state_next_sequence(
    relinow_state_t* state,
    uint8_t peer_index,
    uint8_t channel_id,
    uint16_t* out_seq
) {
    relinow_channel_state_t* channel;
    if (state == 0 || out_seq == 0) {
        return RELINOW_STATE_ERR_INVALID_ARG;
    }
    channel = relinow_find_channel_mut(state, peer_index, channel_id);
    if (channel == 0) {
        return RELINOW_STATE_ERR_NOT_FOUND;
    }

    *out_seq = channel->next_tx_seq;
    channel->next_tx_seq = relinow_seq_next(channel->next_tx_seq);
    return RELINOW_STATE_OK;
}

relinow_state_err_t relinow_state_mark_inflight(
    relinow_state_t* state,
    uint8_t peer_index,
    uint8_t channel_id,
    uint16_t seq_id
) {
    relinow_channel_state_t* channel;
    if (state == 0) {
        return RELINOW_STATE_ERR_INVALID_ARG;
    }
    channel = relinow_find_channel_mut(state, peer_index, channel_id);
    if (channel == 0) {
        return RELINOW_STATE_ERR_NOT_FOUND;
    }

    if (channel->mode == RELINOW_MODE_PRIORITY && channel->has_inflight) {
        channel->inflight_seq = seq_id;
        return RELINOW_STATE_OK;
    }

    channel->has_inflight = 1u;
    channel->inflight_seq = seq_id;
    return RELINOW_STATE_OK;
}

relinow_state_err_t relinow_state_clear_inflight(
    relinow_state_t* state,
    uint8_t peer_index,
    uint8_t channel_id,
    uint16_t seq_id
) {
    relinow_channel_state_t* channel;
    if (state == 0) {
        return RELINOW_STATE_ERR_INVALID_ARG;
    }
    channel = relinow_find_channel_mut(state, peer_index, channel_id);
    if (channel == 0) {
        return RELINOW_STATE_ERR_NOT_FOUND;
    }

    if (channel->has_inflight && channel->inflight_seq == seq_id) {
        channel->has_inflight = 0u;
    }
    return RELINOW_STATE_OK;
}

relinow_state_err_t relinow_state_enqueue_tx(
    relinow_state_t* state,
    uint8_t peer_index,
    uint8_t channel_id,
    uint16_t seq_id
) {
    uint8_t i;
    relinow_channel_state_t* channel;
    if (state == 0) {
        return RELINOW_STATE_ERR_INVALID_ARG;
    }
    channel = relinow_find_channel_mut(state, peer_index, channel_id);
    if (channel == 0) {
        return RELINOW_STATE_ERR_NOT_FOUND;
    }

    for (i = 0; i < RELINOW_TX_QUEUE_SIZE; ++i) {
        if (!channel->tx_queue[i].used) {
            channel->tx_queue[i].used = 1u;
            channel->tx_queue[i].seq_id = seq_id;
            return RELINOW_STATE_OK;
        }
    }
    return RELINOW_STATE_ERR_QUEUE_FULL;
}

relinow_state_err_t relinow_state_enqueue_rx(
    relinow_state_t* state,
    uint8_t peer_index,
    uint8_t channel_id,
    uint16_t seq_id
) {
    uint8_t i;
    relinow_channel_state_t* channel;
    if (state == 0) {
        return RELINOW_STATE_ERR_INVALID_ARG;
    }
    channel = relinow_find_channel_mut(state, peer_index, channel_id);
    if (channel == 0) {
        return RELINOW_STATE_ERR_NOT_FOUND;
    }

    for (i = 0; i < RELINOW_RX_QUEUE_SIZE; ++i) {
        if (!channel->rx_queue[i].used) {
            channel->rx_queue[i].used = 1u;
            channel->rx_queue[i].seq_id = seq_id;
            return RELINOW_STATE_OK;
        }
    }
    return RELINOW_STATE_ERR_QUEUE_FULL;
}
