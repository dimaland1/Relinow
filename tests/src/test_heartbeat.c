#include <stdio.h>
#include <assert.h>
#include "relinow_state.h"

int main() {
    relinow_state_t state;
    relinow_state_init(&state);

    uint8_t mac[6] = {0, 1, 2, 3, 4, 5};
    uint8_t peer_idx = 0;
    relinow_state_add_peer(&state, mac, &peer_idx);

    relinow_state_configure_heartbeat(&state, peer_idx, 2000, 3);

    uint8_t should_ping = 0;
    uint8_t peer_timeout = 0;

    // Test 1: No ping before interval
    relinow_state_poll_heartbeat(&state, peer_idx, 1000, &should_ping, &peer_timeout);
    assert(should_ping == 0);
    assert(peer_timeout == 0);

    // Test 2: Ping after interval
    relinow_state_poll_heartbeat(&state, peer_idx, 2500, &should_ping, &peer_timeout);
    assert(should_ping == 1);
    assert(peer_timeout == 0);

    // Simulate ping sent, next interval
    relinow_state_poll_heartbeat(&state, peer_idx, 4500, &should_ping, &peer_timeout);
    assert(should_ping == 1);
    assert(peer_timeout == 0);

    // Test 3: Pong resets missed
    relinow_state_on_pong(&state, peer_idx);
    
    // Fast forward to timeout
    relinow_state_poll_heartbeat(&state, peer_idx, 6500, &should_ping, &peer_timeout);
    assert(should_ping == 1 && peer_timeout == 0); // missed = 1
    relinow_state_poll_heartbeat(&state, peer_idx, 8500, &should_ping, &peer_timeout);
    assert(should_ping == 1 && peer_timeout == 0); // missed = 2
    relinow_state_poll_heartbeat(&state, peer_idx, 10500, &should_ping, &peer_timeout);
    assert(should_ping == 1 && peer_timeout == 0); // missed = 3
    relinow_state_poll_heartbeat(&state, peer_idx, 12500, &should_ping, &peer_timeout);
    assert(should_ping == 0 && peer_timeout == 1); // timeout!

    printf("test_heartbeat passed\n");
    return 0;
}

