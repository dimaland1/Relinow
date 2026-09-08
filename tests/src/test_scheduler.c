#include <stdio.h>
#include <assert.h>
#include "relinow_state.h"

int main() {
    relinow_state_t state;
    relinow_state_init(&state);

    uint8_t mac[6] = {0, 1, 2, 3, 4, 5};
    uint8_t peer_idx = 0;
    relinow_state_add_peer(&state, mac, &peer_idx);

    relinow_state_open_channel(&state, peer_idx, 0, RELINOW_MODE_RELIABLE, 10);
    relinow_state_open_channel(&state, peer_idx, 1, RELINOW_MODE_RELIABLE, 10);
    relinow_state_open_channel(&state, peer_idx, 2, RELINOW_MODE_UNRELIABLE, 10);

    // Send some reliable packets for channel 0 and 1
    relinow_reliable_tx_result_t prep_tx;
    relinow_state_reliable_send(&state, peer_idx, 0, 1000, &prep_tx); // chan 0
    relinow_state_reliable_send(&state, peer_idx, 1, 1000, &prep_tx); // chan 1

    uint8_t out_channel_id = 0xFF;
    relinow_reliable_tx_result_t out_tx;

    // Test Round Robin (Timeout defaults to approx 50ms)
    relinow_state_scheduler_next(&state, peer_idx, 2000, &out_channel_id, &out_tx);
    assert(out_tx.event == RELINOW_RELIABLE_TX_RETRANSMIT);
    assert(out_channel_id == 0 || out_channel_id == 1);
    uint8_t first_chan = out_channel_id;

    out_tx.event = RELINOW_RELIABLE_TX_NONE;
    relinow_state_scheduler_next(&state, peer_idx, 2000, &out_channel_id, &out_tx);
    assert(out_tx.event == RELINOW_RELIABLE_TX_RETRANSMIT);
    assert(out_channel_id != first_chan); // Should alternate

    printf("test_scheduler passed\n");
    return 0;
}

