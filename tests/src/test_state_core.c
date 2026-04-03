#include "relinow_state.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void fill_mac(uint8_t mac[6], uint8_t seed) {
    uint8_t i;
    for (i = 0; i < 6u; ++i) {
        mac[i] = (uint8_t)(seed + i);
    }
}

static void test_peer_and_channel_tables(void) {
    relinow_state_t state;
    relinow_state_limits_t limits;
    uint8_t idx = 0u;
    uint8_t mac[6];
    uint8_t i;

    relinow_state_init(&state);
    relinow_state_get_limits(&limits);

    assert(limits.max_peers == RELINOW_MAX_PEERS);
    assert(limits.max_channels_per_peer == RELINOW_MAX_CHANNELS_PER_PEER);
    assert(limits.tx_queue_size == RELINOW_TX_QUEUE_SIZE);
    assert(limits.rx_queue_size == RELINOW_RX_QUEUE_SIZE);
    assert(limits.state_footprint_bytes == sizeof(relinow_state_t));

    for (i = 0u; i < RELINOW_MAX_PEERS; ++i) {
        fill_mac(mac, (uint8_t)(10u + i));
        assert(relinow_state_add_peer(&state, mac, &idx) == RELINOW_STATE_OK);
        assert(idx == i);
    }

    fill_mac(mac, 200u);
    assert(relinow_state_add_peer(&state, mac, &idx) == RELINOW_STATE_ERR_NO_SPACE);

    assert(relinow_state_open_channel(&state, 0u, RELINOW_HEARTBEAT_CHANNEL, RELINOW_MODE_RELIABLE, 0u) == RELINOW_STATE_ERR_INVALID_ARG);
    assert(relinow_state_open_channel(&state, 0u, 1u, RELINOW_MODE_RELIABLE, 5u) == RELINOW_STATE_OK);
    assert(relinow_state_open_channel(&state, 0u, 1u, RELINOW_MODE_RELIABLE, 9u) == RELINOW_STATE_OK);
    assert(relinow_state_open_channel(&state, 0u, 1u, RELINOW_MODE_PRIORITY, 9u) == RELINOW_STATE_ERR_CONFLICT);
}

static void test_sequences_and_inflight(void) {
    relinow_state_t state;
    uint8_t mac[6];
    uint8_t peer_idx = 0u;
    uint16_t seq = 0u;

    relinow_state_init(&state);
    fill_mac(mac, 1u);
    assert(relinow_state_add_peer(&state, mac, &peer_idx) == RELINOW_STATE_OK);
    assert(relinow_state_open_channel(&state, peer_idx, 2u, RELINOW_MODE_PRIORITY, 0u) == RELINOW_STATE_OK);

    assert(relinow_state_next_sequence(&state, peer_idx, 2u, &seq) == RELINOW_STATE_OK);
    assert(seq == 0u);

    {
        relinow_channel_state_t* ch = &state.peers[peer_idx].channels[0];
        ch->next_tx_seq = 65535u;
    }

    assert(relinow_state_next_sequence(&state, peer_idx, 2u, &seq) == RELINOW_STATE_OK);
    assert(seq == 65535u);
    assert(relinow_state_next_sequence(&state, peer_idx, 2u, &seq) == RELINOW_STATE_OK);
    assert(seq == 0u);

    assert(relinow_state_mark_inflight(&state, peer_idx, 2u, 100u) == RELINOW_STATE_OK);
    assert(state.peers[peer_idx].channels[0].has_inflight == 1u);
    assert(state.peers[peer_idx].channels[0].inflight_seq == 100u);

    assert(relinow_state_mark_inflight(&state, peer_idx, 2u, 101u) == RELINOW_STATE_OK);
    assert(state.peers[peer_idx].channels[0].inflight_seq == 101u);

    assert(relinow_state_clear_inflight(&state, peer_idx, 2u, 100u) == RELINOW_STATE_OK);
    assert(state.peers[peer_idx].channels[0].has_inflight == 1u);
    assert(relinow_state_clear_inflight(&state, peer_idx, 2u, 101u) == RELINOW_STATE_OK);
    assert(state.peers[peer_idx].channels[0].has_inflight == 0u);
}

static void test_queues(void) {
    relinow_state_t state;
    uint8_t mac[6];
    uint8_t peer_idx = 0u;
    uint8_t i;

    relinow_state_init(&state);
    fill_mac(mac, 50u);
    assert(relinow_state_add_peer(&state, mac, &peer_idx) == RELINOW_STATE_OK);
    assert(relinow_state_open_channel(&state, peer_idx, 3u, RELINOW_MODE_RELIABLE, 0u) == RELINOW_STATE_OK);

    for (i = 0u; i < RELINOW_TX_QUEUE_SIZE; ++i) {
        assert(relinow_state_enqueue_tx(&state, peer_idx, 3u, i) == RELINOW_STATE_OK);
    }
    assert(relinow_state_enqueue_tx(&state, peer_idx, 3u, 99u) == RELINOW_STATE_ERR_QUEUE_FULL);

    for (i = 0u; i < RELINOW_RX_QUEUE_SIZE; ++i) {
        assert(relinow_state_enqueue_rx(&state, peer_idx, 3u, i) == RELINOW_STATE_OK);
    }
    assert(relinow_state_enqueue_rx(&state, peer_idx, 3u, 99u) == RELINOW_STATE_ERR_QUEUE_FULL);
}

int main(void) {
    test_peer_and_channel_tables();
    test_sequences_and_inflight();
    test_queues();

    printf("state core tests ok\n");
    return 0;
}
