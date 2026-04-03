#include "relinow_state.h"

#include <assert.h>
#include <stdio.h>

static void test_unreliable_send_sequence(void) {
    relinow_state_t state;
    uint8_t mac[6] = {0x70u, 0x71u, 0x72u, 0x73u, 0x74u, 0x75u};
    uint8_t peer_idx = 0u;
    uint16_t seq0 = 0u;
    uint16_t seq1 = 0u;

    relinow_state_init(&state);
    assert(relinow_state_add_peer(&state, mac, &peer_idx) == RELINOW_STATE_OK);
    assert(relinow_state_open_channel(&state, peer_idx, 2u, RELINOW_MODE_UNRELIABLE, 0u) == RELINOW_STATE_OK);

    assert(relinow_state_unreliable_send(&state, peer_idx, 2u, &seq0) == RELINOW_STATE_OK);
    assert(relinow_state_unreliable_send(&state, peer_idx, 2u, &seq1) == RELINOW_STATE_OK);
    assert(seq0 == 0u);
    assert(seq1 == 1u);
}

static void test_priority_replacement(void) {
    relinow_state_t state;
    uint8_t mac[6] = {0x80u, 0x81u, 0x82u, 0x83u, 0x84u, 0x85u};
    uint8_t peer_idx = 0u;
    uint16_t seq = 0u;
    uint8_t replaced = 0u;
    uint16_t replaced_seq = 0u;

    relinow_state_init(&state);
    assert(relinow_state_add_peer(&state, mac, &peer_idx) == RELINOW_STATE_OK);
    assert(relinow_state_open_channel(&state, peer_idx, 3u, RELINOW_MODE_PRIORITY, 0u) == RELINOW_STATE_OK);

    assert(relinow_state_priority_send(&state, peer_idx, 3u, &seq, &replaced, &replaced_seq) == RELINOW_STATE_OK);
    assert(seq == 0u);
    assert(replaced == 0u);

    assert(relinow_state_priority_send(&state, peer_idx, 3u, &seq, &replaced, &replaced_seq) == RELINOW_STATE_OK);
    assert(seq == 1u);
    assert(replaced == 1u);
    assert(replaced_seq == 0u);

    assert(relinow_state_clear_inflight_any(&state, peer_idx, 3u) == RELINOW_STATE_OK);

    assert(relinow_state_priority_send(&state, peer_idx, 3u, &seq, &replaced, &replaced_seq) == RELINOW_STATE_OK);
    assert(seq == 2u);
    assert(replaced == 0u);
}

static void test_priority_rx_newest_wins(void) {
    relinow_state_t state;
    uint8_t mac[6] = {0x90u, 0x91u, 0x92u, 0x93u, 0x94u, 0x95u};
    uint8_t peer_idx = 0u;
    uint8_t should_deliver = 0u;

    relinow_state_init(&state);
    assert(relinow_state_add_peer(&state, mac, &peer_idx) == RELINOW_STATE_OK);
    assert(relinow_state_open_channel(&state, peer_idx, 4u, RELINOW_MODE_PRIORITY, 0u) == RELINOW_STATE_OK);

    assert(relinow_state_priority_on_data(&state, peer_idx, 4u, 10u, &should_deliver) == RELINOW_STATE_OK);
    assert(should_deliver == 1u);

    assert(relinow_state_priority_on_data(&state, peer_idx, 4u, 10u, &should_deliver) == RELINOW_STATE_OK);
    assert(should_deliver == 0u);

    assert(relinow_state_priority_on_data(&state, peer_idx, 4u, 9u, &should_deliver) == RELINOW_STATE_OK);
    assert(should_deliver == 0u);

    assert(relinow_state_priority_on_data(&state, peer_idx, 4u, 11u, &should_deliver) == RELINOW_STATE_OK);
    assert(should_deliver == 1u);
}

static void test_wrong_mode_guards(void) {
    relinow_state_t state;
    uint8_t mac[6] = {0xA0u, 0xA1u, 0xA2u, 0xA3u, 0xA4u, 0xA5u};
    uint8_t peer_idx = 0u;
    uint16_t seq = 0u;
    uint8_t replaced = 0u;
    uint16_t replaced_seq = 0u;
    uint8_t should_deliver = 0u;

    relinow_state_init(&state);
    assert(relinow_state_add_peer(&state, mac, &peer_idx) == RELINOW_STATE_OK);
    assert(relinow_state_open_channel(&state, peer_idx, 5u, RELINOW_MODE_RELIABLE, 0u) == RELINOW_STATE_OK);

    assert(relinow_state_unreliable_send(&state, peer_idx, 5u, &seq) == RELINOW_STATE_ERR_WRONG_MODE);
    assert(relinow_state_priority_send(&state, peer_idx, 5u, &seq, &replaced, &replaced_seq) == RELINOW_STATE_ERR_WRONG_MODE);
    assert(relinow_state_priority_on_data(&state, peer_idx, 5u, 1u, &should_deliver) == RELINOW_STATE_ERR_WRONG_MODE);
}

int main(void) {
    test_unreliable_send_sequence();
    test_priority_replacement();
    test_priority_rx_newest_wins();
    test_wrong_mode_guards();

    printf("non-reliable mode tests ok\n");
    return 0;
}
