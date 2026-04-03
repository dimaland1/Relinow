#include "relinow_state.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static relinow_channel_state_t* find_channel_mut(relinow_state_t* state, uint8_t peer_index, uint8_t channel_id) {
    uint8_t i;
    relinow_peer_state_t* peer = &state->peers[peer_index];
    for (i = 0u; i < RELINOW_MAX_CHANNELS_PER_PEER; ++i) {
        relinow_channel_state_t* channel = &peer->channels[i];
        if (channel->in_use && channel->channel_id == channel_id) {
            return channel;
        }
    }
    return 0;
}

static void test_reliable_state_send_ack_poll(void) {
    relinow_state_t state;
    relinow_reliable_config_t cfg;
    relinow_reliable_tx_result_t tx;
    uint8_t peer_idx = 0u;
    uint8_t mac[6] = {0x24u, 0x6Fu, 0x28u, 0xAAu, 0xBBu, 0xCCu};
    uint16_t rtt_ms = 0u;
    relinow_channel_state_t* channel;

    relinow_state_init(&state);
    assert(relinow_state_add_peer(&state, mac, &peer_idx) == RELINOW_STATE_OK);
    assert(relinow_state_open_channel(&state, peer_idx, 7u, RELINOW_MODE_RELIABLE, 0u) == RELINOW_STATE_OK);

    relinow_reliable_default_config(&cfg);
    cfg.max_retries = 2u;
    cfg.initial_rtt_ms = 50u;
    cfg.rtt_alpha_q8 = 32u;
    assert(relinow_state_set_reliable_config(&state, peer_idx, 7u, &cfg) == RELINOW_STATE_OK);

    assert(relinow_state_reliable_send(&state, peer_idx, 7u, 0u, &tx) == RELINOW_STATE_OK);
    assert(tx.event == RELINOW_RELIABLE_TX_NEW);
    assert(tx.seq_id == 0u);

    assert(relinow_state_reliable_poll(&state, peer_idx, 7u, 74u, &tx) == RELINOW_STATE_OK);
    assert(tx.event == RELINOW_RELIABLE_TX_NONE);

    assert(relinow_state_reliable_poll(&state, peer_idx, 7u, 75u, &tx) == RELINOW_STATE_OK);
    assert(tx.event == RELINOW_RELIABLE_TX_RETRANSMIT);
    assert(tx.seq_id == 0u);

    assert(relinow_state_reliable_on_ack(&state, peer_idx, 7u, 0u, 95u) == RELINOW_STATE_OK);
    assert(relinow_state_reliable_get_rtt_ms(&state, peer_idx, 7u, &rtt_ms) == RELINOW_STATE_OK);
    assert(rtt_ms < 50u);

    channel = find_channel_mut(&state, peer_idx, 7u);
    assert(channel != 0);
    assert(channel->has_inflight == 0u);

    assert(relinow_state_reliable_send(&state, peer_idx, 7u, 100u, &tx) == RELINOW_STATE_OK);
    assert(tx.event == RELINOW_RELIABLE_TX_NEW);
    assert(tx.seq_id == 1u);

    assert(relinow_state_reliable_on_ack(&state, peer_idx, 7u, 1u, 108u) == RELINOW_STATE_OK);
    assert(relinow_state_reliable_get_rtt_ms(&state, peer_idx, 7u, &rtt_ms) == RELINOW_STATE_OK);
    assert(rtt_ms < 45u);
}

static void test_reliable_state_ordering_and_duplicates(void) {
    relinow_state_t state;
    relinow_reliable_rx_result_t rx;
    uint8_t peer_idx = 0u;
    uint8_t mac[6] = {0x30u, 0x31u, 0x32u, 0x33u, 0x34u, 0x35u};
    relinow_channel_state_t* channel;

    relinow_state_init(&state);
    assert(relinow_state_add_peer(&state, mac, &peer_idx) == RELINOW_STATE_OK);
    assert(relinow_state_open_channel(&state, peer_idx, 3u, RELINOW_MODE_RELIABLE, 0u) == RELINOW_STATE_OK);

    assert(relinow_state_reliable_on_data(&state, peer_idx, 3u, 10u, &rx) == RELINOW_STATE_OK);
    assert(rx.status == RELINOW_RELIABLE_RX_DELIVERED);
    assert(rx.delivered_count == 1u);

    assert(relinow_state_reliable_on_data(&state, peer_idx, 3u, 12u, &rx) == RELINOW_STATE_OK);
    assert(rx.status == RELINOW_RELIABLE_RX_OUT_OF_ORDER);
    assert(rx.delivered_count == 0u);

    assert(relinow_state_reliable_on_data(&state, peer_idx, 3u, 10u, &rx) == RELINOW_STATE_OK);
    assert(rx.status == RELINOW_RELIABLE_RX_DUPLICATE);
    assert(rx.delivered_count == 0u);

    assert(relinow_state_reliable_on_data(&state, peer_idx, 3u, 11u, &rx) == RELINOW_STATE_OK);
    assert(rx.status == RELINOW_RELIABLE_RX_DELIVERED);
    assert(rx.delivered_count == 2u);
    assert(rx.delivered_seq[0] == 11u);
    assert(rx.delivered_seq[1] == 12u);

    channel = find_channel_mut(&state, peer_idx, 3u);
    assert(channel != 0);
    assert(channel->rx_queue[0].used == 1u && channel->rx_queue[0].seq_id == 10u);
    assert(channel->rx_queue[1].used == 1u && channel->rx_queue[1].seq_id == 11u);
    assert(channel->rx_queue[2].used == 1u && channel->rx_queue[2].seq_id == 12u);
}

static void test_wrong_mode_guards(void) {
    relinow_state_t state;
    relinow_reliable_tx_result_t tx;
    relinow_reliable_rx_result_t rx;
    uint8_t peer_idx = 0u;
    uint8_t mac[6] = {0x40u, 0x41u, 0x42u, 0x43u, 0x44u, 0x45u};

    relinow_state_init(&state);
    assert(relinow_state_add_peer(&state, mac, &peer_idx) == RELINOW_STATE_OK);
    assert(relinow_state_open_channel(&state, peer_idx, 9u, RELINOW_MODE_UNRELIABLE, 0u) == RELINOW_STATE_OK);

    assert(relinow_state_reliable_send(&state, peer_idx, 9u, 0u, &tx) == RELINOW_STATE_ERR_WRONG_MODE);
    assert(relinow_state_reliable_on_data(&state, peer_idx, 9u, 1u, &rx) == RELINOW_STATE_ERR_WRONG_MODE);
}

int main(void) {
    test_reliable_state_send_ack_poll();
    test_reliable_state_ordering_and_duplicates();
    test_wrong_mode_guards();

    printf("reliable state integration tests ok\n");
    return 0;
}
