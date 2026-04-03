#ifndef RELINOW_STATE_H
#define RELINOW_STATE_H

#include <stddef.h>
#include <stdint.h>

#include "relinow_packet.h"

#define RELINOW_MAX_PEERS 20u
#define RELINOW_MAX_CHANNELS_PER_PEER 16u
#define RELINOW_TX_QUEUE_SIZE 16u
#define RELINOW_RX_QUEUE_SIZE 8u

typedef enum {
    RELINOW_STATE_OK = 0,
    RELINOW_STATE_ERR_INVALID_ARG = 1,
    RELINOW_STATE_ERR_NOT_FOUND = 2,
    RELINOW_STATE_ERR_NO_SPACE = 3,
    RELINOW_STATE_ERR_CONFLICT = 4,
    RELINOW_STATE_ERR_QUEUE_FULL = 5
} relinow_state_err_t;

typedef struct {
    uint8_t used;
    uint16_t seq_id;
} relinow_tx_slot_t;

typedef struct {
    uint8_t used;
    uint16_t seq_id;
} relinow_rx_slot_t;

typedef struct {
    uint8_t in_use;
    uint8_t channel_id;
    uint8_t mode;
    uint8_t priority;
    uint16_t next_tx_seq;
    uint16_t expected_rx_seq;
    uint8_t has_inflight;
    uint16_t inflight_seq;
    relinow_tx_slot_t tx_queue[RELINOW_TX_QUEUE_SIZE];
    relinow_rx_slot_t rx_queue[RELINOW_RX_QUEUE_SIZE];
} relinow_channel_state_t;

typedef struct {
    uint8_t in_use;
    uint8_t mac[6];
    relinow_channel_state_t channels[RELINOW_MAX_CHANNELS_PER_PEER];
} relinow_peer_state_t;

typedef struct {
    relinow_peer_state_t peers[RELINOW_MAX_PEERS];
} relinow_state_t;

typedef struct {
    uint16_t max_peers;
    uint16_t max_channels_per_peer;
    uint16_t tx_queue_size;
    uint16_t rx_queue_size;
    size_t state_footprint_bytes;
} relinow_state_limits_t;

void relinow_state_init(relinow_state_t* state);

void relinow_state_get_limits(relinow_state_limits_t* out_limits);

relinow_state_err_t relinow_state_add_peer(
    relinow_state_t* state,
    const uint8_t mac[6],
    uint8_t* out_peer_index
);

relinow_state_err_t relinow_state_find_peer(
    const relinow_state_t* state,
    const uint8_t mac[6],
    uint8_t* out_peer_index
);

relinow_state_err_t relinow_state_open_channel(
    relinow_state_t* state,
    uint8_t peer_index,
    uint8_t channel_id,
    uint8_t mode,
    uint8_t priority
);

relinow_state_err_t relinow_state_next_sequence(
    relinow_state_t* state,
    uint8_t peer_index,
    uint8_t channel_id,
    uint16_t* out_seq
);

relinow_state_err_t relinow_state_mark_inflight(
    relinow_state_t* state,
    uint8_t peer_index,
    uint8_t channel_id,
    uint16_t seq_id
);

relinow_state_err_t relinow_state_clear_inflight(
    relinow_state_t* state,
    uint8_t peer_index,
    uint8_t channel_id,
    uint16_t seq_id
);

relinow_state_err_t relinow_state_enqueue_tx(
    relinow_state_t* state,
    uint8_t peer_index,
    uint8_t channel_id,
    uint16_t seq_id
);

relinow_state_err_t relinow_state_enqueue_rx(
    relinow_state_t* state,
    uint8_t peer_index,
    uint8_t channel_id,
    uint16_t seq_id
);

#endif
