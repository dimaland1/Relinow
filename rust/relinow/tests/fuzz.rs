use proptest::prelude::*;
use relinow::{packet::RelinowHeader, state::RelinowState, reliable::default_reliable_config};
use relinow_sys::*;

proptest! {
    // 1. Fuzzing the Packet Decoder
    // We throw completely random byte arrays of random lengths (0 to 300)
    // to prove that `decode()` NEVER panics, no matter what garbage it receives.
    #[test]
    fn fuzz_packet_decode(frame in prop::collection::vec(any::<u8>(), 0..300), max_payload in 0..500u16) {
        // This is safe Rust. Even if the payload is garbage, 
        // it must gracefully return an Err(InvalidPacket), and NEVER crash.
        let _ = RelinowHeader::decode(&frame, max_payload);
    }

    // 2. Fuzzing the State Machine (Reliable Channel)
    // We create a valid channel and bombard it with random sequence numbers and random acks.
    #[test]
    fn fuzz_state_machine_reliable(
        random_acks in prop::collection::vec(any::<u16>(), 0..100),
        random_seqs in prop::collection::vec(any::<u16>(), 0..100),
        random_time_jumps in prop::collection::vec(1..1000u32, 0..100)
    ) {
        let mut state = RelinowState::new();
        let mac = [0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF];
        
        // Setup
        let peer = state.add_peer(&mac).expect("Should add peer");
        state.open_channel(peer, 5, RELINOW_MODE_RELIABLE, 10).expect("Should open channel");
        
        let cfg = default_reliable_config();
        state.set_reliable_config(peer, 5, &cfg).expect("Should configure reliable");

        let mut now = 1000u32;

        // Fuzz loop
        let max_iters = random_acks.len().min(random_time_jumps.len()).min(random_seqs.len());
        for i in 0..max_iters {
            now = now.wrapping_add(random_time_jumps[i]);
            
            // Randomly acknowledge arbitrary packets
            let _ = state.reliable_on_ack(peer, 5, random_acks[i], now);
            
            // Randomly receive arbitrary data packets
            let _ = state.reliable_on_data(peer, 5, random_seqs[i]);
            
            // Randomly attempt to send and poll
            let _ = state.reliable_send(peer, 5, now);
            let _ = state.reliable_poll(peer, 5, now);
            let _ = state.scheduler_next(peer, now);
        }
    }
}
