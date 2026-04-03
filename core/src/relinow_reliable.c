#include "relinow_reliable.h"

#include <string.h>

static uint16_t relinow_compute_timeout_ms(uint16_t rtt_ms, uint8_t retry_index) {
    uint32_t timeout;
    if (retry_index == 0u) {
        timeout = ((uint32_t)rtt_ms * 3u) / 2u;
    } else if (retry_index == 1u) {
        timeout = (uint32_t)rtt_ms * 2u;
    } else {
        timeout = (uint32_t)rtt_ms * 3u;
    }
    if (timeout == 0u) {
        timeout = 1u;
    }
    if (timeout > 65535u) {
        timeout = 65535u;
    }
    return (uint16_t)timeout;
}

static relinow_reliable_pending_t* relinow_find_pending_slot(relinow_reliable_ctx_t* ctx, uint16_t seq_id) {
    uint8_t i;
    for (i = 0u; i < RELINOW_RELIABLE_MAX_PENDING; ++i) {
        if (ctx->pending[i].in_use && ctx->pending[i].seq_id == seq_id) {
            return &ctx->pending[i];
        }
    }
    return 0;
}

static relinow_reliable_pending_t* relinow_find_free_pending_slot(relinow_reliable_ctx_t* ctx) {
    uint8_t i;
    for (i = 0u; i < RELINOW_RELIABLE_MAX_PENDING; ++i) {
        if (!ctx->pending[i].in_use) {
            return &ctx->pending[i];
        }
    }
    return 0;
}

static int relinow_reorder_contains(const relinow_reliable_ctx_t* ctx, uint16_t seq_id) {
    uint8_t i;
    for (i = 0u; i < RELINOW_RELIABLE_MAX_REORDER; ++i) {
        if (ctx->reorder[i].in_use && ctx->reorder[i].seq_id == seq_id) {
            return 1;
        }
    }
    return 0;
}

static int relinow_reorder_insert(relinow_reliable_ctx_t* ctx, uint16_t seq_id) {
    uint8_t i;
    for (i = 0u; i < RELINOW_RELIABLE_MAX_REORDER; ++i) {
        if (!ctx->reorder[i].in_use) {
            ctx->reorder[i].in_use = 1u;
            ctx->reorder[i].seq_id = seq_id;
            return 1;
        }
    }
    return 0;
}

static int relinow_reorder_take_expected(relinow_reliable_ctx_t* ctx, uint16_t expected) {
    uint8_t i;
    for (i = 0u; i < RELINOW_RELIABLE_MAX_REORDER; ++i) {
        if (ctx->reorder[i].in_use && ctx->reorder[i].seq_id == expected) {
            ctx->reorder[i].in_use = 0u;
            return 1;
        }
    }
    return 0;
}

void relinow_reliable_default_config(relinow_reliable_config_t* out_cfg) {
    if (out_cfg == 0) {
        return;
    }
    out_cfg->max_retries = 3u;
    out_cfg->initial_rtt_ms = 50u;
    out_cfg->rtt_alpha_q8 = 32u;
}

void relinow_reliable_init(relinow_reliable_ctx_t* ctx, const relinow_reliable_config_t* cfg) {
    relinow_reliable_config_t default_cfg;
    if (ctx == 0) {
        return;
    }

    relinow_reliable_default_config(&default_cfg);
    memset(ctx, 0, sizeof(*ctx));
    if (cfg != 0) {
        ctx->cfg = *cfg;
    } else {
        ctx->cfg = default_cfg;
    }

    if (ctx->cfg.max_retries == 0u) {
        ctx->cfg.max_retries = 1u;
    }
    if (ctx->cfg.initial_rtt_ms == 0u) {
        ctx->cfg.initial_rtt_ms = default_cfg.initial_rtt_ms;
    }
    if (ctx->cfg.rtt_alpha_q8 == 0u) {
        ctx->cfg.rtt_alpha_q8 = default_cfg.rtt_alpha_q8;
    }

    ctx->rtt_estimate_ms = ctx->cfg.initial_rtt_ms;
}

relinow_err_t relinow_reliable_send(
    relinow_reliable_ctx_t* ctx,
    uint32_t now_ms,
    relinow_reliable_tx_result_t* out_result
) {
    relinow_reliable_pending_t* slot;
    if (ctx == 0 || out_result == 0) {
        return RELINOW_ERR_INVALID_ARG;
    }

    slot = relinow_find_free_pending_slot(ctx);
    if (slot == 0) {
        return RELINOW_ERR_INVALID_PACKET;
    }

    memset(slot, 0, sizeof(*slot));
    slot->in_use = 1u;
    slot->seq_id = ctx->next_tx_seq;
    slot->last_send_ms = now_ms;
    slot->timeout_ms = relinow_compute_timeout_ms(ctx->rtt_estimate_ms, 0u);
    slot->retries = 0u;

    out_result->event = RELINOW_RELIABLE_TX_NEW;
    out_result->seq_id = slot->seq_id;

    ctx->next_tx_seq = relinow_seq_next(ctx->next_tx_seq);
    return RELINOW_ERR_OK;
}

relinow_reliable_tx_event_t relinow_reliable_on_ack(
    relinow_reliable_ctx_t* ctx,
    uint16_t ack_id,
    uint32_t now_ms
) {
    uint32_t sample;
    uint32_t old_part;
    uint32_t new_part;
    relinow_reliable_pending_t* slot;

    if (ctx == 0) {
        return RELINOW_RELIABLE_TX_NONE;
    }

    slot = relinow_find_pending_slot(ctx, ack_id);
    if (slot == 0) {
        return RELINOW_RELIABLE_TX_NONE;
    }

    sample = now_ms - slot->last_send_ms;
    if (sample > 65535u) {
        sample = 65535u;
    }

    old_part = (uint32_t)(256u - ctx->cfg.rtt_alpha_q8) * ctx->rtt_estimate_ms;
    new_part = (uint32_t)ctx->cfg.rtt_alpha_q8 * sample;
    ctx->rtt_estimate_ms = (uint16_t)((old_part + new_part) / 256u);
    if (ctx->rtt_estimate_ms == 0u) {
        ctx->rtt_estimate_ms = 1u;
    }

    slot->in_use = 0u;
    return RELINOW_RELIABLE_TX_NONE;
}

relinow_err_t relinow_reliable_poll(
    relinow_reliable_ctx_t* ctx,
    uint32_t now_ms,
    relinow_reliable_tx_result_t* out_result
) {
    uint8_t i;
    if (ctx == 0 || out_result == 0) {
        return RELINOW_ERR_INVALID_ARG;
    }

    out_result->event = RELINOW_RELIABLE_TX_NONE;
    out_result->seq_id = 0u;

    for (i = 0u; i < RELINOW_RELIABLE_MAX_PENDING; ++i) {
        relinow_reliable_pending_t* slot = &ctx->pending[i];
        uint32_t elapsed;
        if (!slot->in_use) {
            continue;
        }

        elapsed = now_ms - slot->last_send_ms;
        if (elapsed < (uint32_t)slot->timeout_ms) {
            continue;
        }

        if (slot->retries >= ctx->cfg.max_retries) {
            out_result->event = RELINOW_RELIABLE_TX_FAILED;
            out_result->seq_id = slot->seq_id;
            slot->in_use = 0u;
            return RELINOW_ERR_OK;
        }

        slot->retries = (uint8_t)(slot->retries + 1u);
        slot->last_send_ms = now_ms;
        slot->timeout_ms = relinow_compute_timeout_ms(ctx->rtt_estimate_ms, slot->retries);

        out_result->event = RELINOW_RELIABLE_TX_RETRANSMIT;
        out_result->seq_id = slot->seq_id;
        return RELINOW_ERR_OK;
    }

    return RELINOW_ERR_OK;
}

relinow_err_t relinow_reliable_on_data(
    relinow_reliable_ctx_t* ctx,
    uint16_t seq_id,
    relinow_reliable_rx_result_t* out_result
) {
    if (ctx == 0 || out_result == 0) {
        return RELINOW_ERR_INVALID_ARG;
    }

    memset(out_result, 0, sizeof(*out_result));

    if (!ctx->has_rx_window) {
        ctx->has_rx_window = 1u;
        ctx->expected_rx_seq = relinow_seq_next(seq_id);
        out_result->status = RELINOW_RELIABLE_RX_DELIVERED;
        out_result->delivered_count = 1u;
        out_result->delivered_seq[0] = seq_id;
        out_result->ack_id = seq_id;
        return RELINOW_ERR_OK;
    }

    if (seq_id == ctx->expected_rx_seq) {
        uint8_t count = 0u;
        out_result->status = RELINOW_RELIABLE_RX_DELIVERED;

        out_result->delivered_seq[count++] = seq_id;
        ctx->expected_rx_seq = relinow_seq_next(ctx->expected_rx_seq);

        while (count < (uint8_t)(1u + RELINOW_RELIABLE_MAX_REORDER) &&
               relinow_reorder_take_expected(ctx, ctx->expected_rx_seq)) {
            out_result->delivered_seq[count++] = ctx->expected_rx_seq;
            ctx->expected_rx_seq = relinow_seq_next(ctx->expected_rx_seq);
        }

        out_result->delivered_count = count;
        out_result->ack_id = (uint16_t)(ctx->expected_rx_seq - 1u);
        return RELINOW_ERR_OK;
    }

    if (relinow_is_seq_newer(seq_id, ctx->expected_rx_seq)) {
        if (relinow_reorder_contains(ctx, seq_id) || relinow_reorder_insert(ctx, seq_id)) {
            out_result->status = RELINOW_RELIABLE_RX_OUT_OF_ORDER;
            out_result->ack_id = (uint16_t)(ctx->expected_rx_seq - 1u);
            return RELINOW_ERR_OK;
        }

        out_result->status = RELINOW_RELIABLE_RX_DROPPED;
        out_result->ack_id = (uint16_t)(ctx->expected_rx_seq - 1u);
        return RELINOW_ERR_OK;
    }

    out_result->status = RELINOW_RELIABLE_RX_DUPLICATE;
    out_result->ack_id = (uint16_t)(ctx->expected_rx_seq - 1u);
    return RELINOW_ERR_OK;
}

uint16_t relinow_reliable_current_rtt_ms(const relinow_reliable_ctx_t* ctx) {
    if (ctx == 0) {
        return 0u;
    }
    return ctx->rtt_estimate_ms;
}

uint16_t relinow_reliable_compute_timeout_ms(const relinow_reliable_ctx_t* ctx, uint8_t retry_index) {
    if (ctx == 0) {
        return 0u;
    }
    return relinow_compute_timeout_ms(ctx->rtt_estimate_ms, retry_index);
}
