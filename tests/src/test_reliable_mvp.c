#include "relinow_reliable.h"

#include <assert.h>
#include <stdio.h>

static void test_retransmission_and_rtt(void) {
    relinow_reliable_ctx_t ctx;
    relinow_reliable_config_t cfg;
    relinow_reliable_tx_result_t tx;

    relinow_reliable_default_config(&cfg);
    cfg.max_retries = 3u;
    cfg.initial_rtt_ms = 50u;
    cfg.rtt_alpha_q8 = 32u;
    relinow_reliable_init(&ctx, &cfg);

    assert(relinow_reliable_send(&ctx, 0u, &tx) == RELINOW_ERR_OK);
    assert(tx.event == RELINOW_RELIABLE_TX_NEW);
    assert(tx.seq_id == 0u);

    assert(relinow_reliable_poll(&ctx, 74u, &tx) == RELINOW_ERR_OK);
    assert(tx.event == RELINOW_RELIABLE_TX_NONE);

    assert(relinow_reliable_poll(&ctx, 75u, &tx) == RELINOW_ERR_OK);
    assert(tx.event == RELINOW_RELIABLE_TX_RETRANSMIT);
    assert(tx.seq_id == 0u);

    assert(relinow_reliable_on_ack(&ctx, 0u, 95u) == RELINOW_RELIABLE_TX_NONE);
    assert(relinow_reliable_current_rtt_ms(&ctx) < 50u);
    assert(relinow_reliable_compute_timeout_ms(&ctx, 0u) < 75u);

    assert(relinow_reliable_poll(&ctx, 400u, &tx) == RELINOW_ERR_OK);
    assert(tx.event == RELINOW_RELIABLE_TX_NONE);
}

static void test_retry_backoff_and_failure(void) {
    relinow_reliable_ctx_t ctx;
    relinow_reliable_config_t cfg;
    relinow_reliable_tx_result_t tx;

    relinow_reliable_default_config(&cfg);
    cfg.max_retries = 2u;
    cfg.initial_rtt_ms = 40u;
    relinow_reliable_init(&ctx, &cfg);

    assert(relinow_reliable_send(&ctx, 0u, &tx) == RELINOW_ERR_OK);
    assert(tx.seq_id == 0u);

    assert(relinow_reliable_poll(&ctx, 60u, &tx) == RELINOW_ERR_OK);
    assert(tx.event == RELINOW_RELIABLE_TX_RETRANSMIT);

    assert(relinow_reliable_poll(&ctx, 139u, &tx) == RELINOW_ERR_OK);
    assert(tx.event == RELINOW_RELIABLE_TX_NONE);

    assert(relinow_reliable_poll(&ctx, 140u, &tx) == RELINOW_ERR_OK);
    assert(tx.event == RELINOW_RELIABLE_TX_RETRANSMIT);

    assert(relinow_reliable_poll(&ctx, 259u, &tx) == RELINOW_ERR_OK);
    assert(tx.event == RELINOW_RELIABLE_TX_NONE);

    assert(relinow_reliable_poll(&ctx, 260u, &tx) == RELINOW_ERR_OK);
    assert(tx.event == RELINOW_RELIABLE_TX_FAILED);
    assert(tx.seq_id == 0u);
}

static void test_ordering_and_duplicates(void) {
    relinow_reliable_ctx_t ctx;
    relinow_reliable_rx_result_t rx;

    relinow_reliable_init(&ctx, 0);

    assert(relinow_reliable_on_data(&ctx, 10u, &rx) == RELINOW_ERR_OK);
    assert(rx.status == RELINOW_RELIABLE_RX_DELIVERED);
    assert(rx.delivered_count == 1u);
    assert(rx.delivered_seq[0] == 10u);

    assert(relinow_reliable_on_data(&ctx, 12u, &rx) == RELINOW_ERR_OK);
    assert(rx.status == RELINOW_RELIABLE_RX_OUT_OF_ORDER);
    assert(rx.delivered_count == 0u);
    assert(rx.ack_id == 10u);

    assert(relinow_reliable_on_data(&ctx, 10u, &rx) == RELINOW_ERR_OK);
    assert(rx.status == RELINOW_RELIABLE_RX_DUPLICATE);
    assert(rx.delivered_count == 0u);

    assert(relinow_reliable_on_data(&ctx, 11u, &rx) == RELINOW_ERR_OK);
    assert(rx.status == RELINOW_RELIABLE_RX_DELIVERED);
    assert(rx.delivered_count == 2u);
    assert(rx.delivered_seq[0] == 11u);
    assert(rx.delivered_seq[1] == 12u);
    assert(rx.ack_id == 12u);
}

int main(void) {
    test_retransmission_and_rtt();
    test_retry_backoff_and_failure();
    test_ordering_and_duplicates();

    printf("reliable mvp tests ok\n");
    return 0;
}
