#ifndef RELINOW_RELIABLE_H
#define RELINOW_RELIABLE_H

#include <stdint.h>

#include "relinow_packet.h"

#define RELINOW_RELIABLE_MAX_PENDING 16u
#define RELINOW_RELIABLE_MAX_REORDER 8u

typedef enum {
    RELINOW_RELIABLE_TX_NONE = 0,
    RELINOW_RELIABLE_TX_NEW = 1,
    RELINOW_RELIABLE_TX_RETRANSMIT = 2,
    RELINOW_RELIABLE_TX_FAILED = 3
} relinow_reliable_tx_event_t;

typedef enum {
    RELINOW_RELIABLE_RX_DELIVERED = 0,
    RELINOW_RELIABLE_RX_OUT_OF_ORDER = 1,
    RELINOW_RELIABLE_RX_DUPLICATE = 2,
    RELINOW_RELIABLE_RX_DROPPED = 3
} relinow_reliable_rx_status_t;

typedef struct {
    uint8_t max_retries;
    uint16_t initial_rtt_ms;
    uint8_t rtt_alpha_q8;
} relinow_reliable_config_t;

typedef struct {
    relinow_reliable_tx_event_t event;
    uint16_t seq_id;
} relinow_reliable_tx_result_t;

typedef struct {
    relinow_reliable_rx_status_t status;
    uint8_t delivered_count;
    uint16_t delivered_seq[1u + RELINOW_RELIABLE_MAX_REORDER];
    uint16_t ack_id;
} relinow_reliable_rx_result_t;

typedef struct {
    uint8_t in_use;
    uint16_t seq_id;
    uint32_t last_send_ms;
    uint16_t timeout_ms;
    uint8_t retries;
} relinow_reliable_pending_t;

typedef struct {
    uint8_t in_use;
    uint16_t seq_id;
} relinow_reliable_reorder_t;

typedef struct {
    relinow_reliable_config_t cfg;
    uint16_t next_tx_seq;
    uint16_t rtt_estimate_ms;
    uint8_t has_rx_window;
    uint16_t expected_rx_seq;
    relinow_reliable_pending_t pending[RELINOW_RELIABLE_MAX_PENDING];
    relinow_reliable_reorder_t reorder[RELINOW_RELIABLE_MAX_REORDER];
} relinow_reliable_ctx_t;

void relinow_reliable_default_config(relinow_reliable_config_t* out_cfg);
void relinow_reliable_init(relinow_reliable_ctx_t* ctx, const relinow_reliable_config_t* cfg);

relinow_err_t relinow_reliable_send(
    relinow_reliable_ctx_t* ctx,
    uint32_t now_ms,
    relinow_reliable_tx_result_t* out_result
);

relinow_reliable_tx_event_t relinow_reliable_on_ack(
    relinow_reliable_ctx_t* ctx,
    uint16_t ack_id,
    uint32_t now_ms
);

relinow_err_t relinow_reliable_poll(
    relinow_reliable_ctx_t* ctx,
    uint32_t now_ms,
    relinow_reliable_tx_result_t* out_result
);

relinow_err_t relinow_reliable_on_data(
    relinow_reliable_ctx_t* ctx,
    uint16_t seq_id,
    relinow_reliable_rx_result_t* out_result
);

uint16_t relinow_reliable_current_rtt_ms(const relinow_reliable_ctx_t* ctx);
uint16_t relinow_reliable_compute_timeout_ms(const relinow_reliable_ctx_t* ctx, uint8_t retry_index);

#endif
