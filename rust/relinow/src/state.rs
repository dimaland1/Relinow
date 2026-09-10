use relinow_sys::*;
use crate::error::StateError;
use core::mem::MaybeUninit;

pub struct RelinowState {
    inner: relinow_state_t,
}

impl RelinowState {
    /// Creates a new, zero-initialized RelinowState.
    pub fn new() -> Self {
        let mut state = MaybeUninit::<relinow_state_t>::uninit();
        unsafe {
            relinow_state_init(state.as_mut_ptr());
            Self {
                inner: state.assume_init(),
            }
        }
    }

    /// Adds a peer by MAC address, returning its peer index.
    pub fn add_peer(&mut self, mac: &[u8; 6]) -> Result<u8, StateError> {
        let mut peer_index = 0;
        let err = unsafe {
            relinow_state_add_peer(&mut self.inner, mac.as_ptr(), &mut peer_index)
        };
        StateError::into_result(err).map(|_| peer_index)
    }

    /// Finds a peer by MAC address, returning its peer index if found.
    pub fn find_peer(&self, mac: &[u8; 6]) -> Result<u8, StateError> {
        let mut peer_index = 0;
        let err = unsafe {
            relinow_state_find_peer(&self.inner, mac.as_ptr(), &mut peer_index)
        };
        StateError::into_result(err).map(|_| peer_index)
    }

    /// Opens a logical channel for a specific peer.
    pub fn open_channel(
        &mut self,
        peer_index: u8,
        channel_id: u8,
        mode: u8,
        priority: u8,
    ) -> Result<(), StateError> {
        let err = unsafe {
            relinow_state_open_channel(&mut self.inner, peer_index, channel_id, mode, priority)
        };
        StateError::into_result(err)
    }

    /// Fetches the current operating mode of a channel.
    pub fn get_channel_mode(&self, peer_index: u8, channel_id: u8) -> Result<u8, StateError> {
        let mut mode = 0;
        let err = unsafe {
            relinow_state_get_channel_mode(&self.inner, peer_index, channel_id, &mut mode)
        };
        StateError::into_result(err).map(|_| mode)
    }

    /// Sets the reliable configuration for a specific channel.
    pub fn set_reliable_config(
        &mut self,
        peer_index: u8,
        channel_id: u8,
        cfg: &relinow_reliable_config_t,
    ) -> Result<(), StateError> {
        let err = unsafe {
            relinow_state_set_reliable_config(&mut self.inner, peer_index, channel_id, cfg)
        };
        StateError::into_result(err)
    }

    /// Queues a new RELIABLE packet for sending.
    pub fn reliable_send(
        &mut self,
        peer_index: u8,
        channel_id: u8,
        now_ms: u32,
    ) -> Result<relinow_reliable_tx_result_t, StateError> {
        let mut result = MaybeUninit::<relinow_reliable_tx_result_t>::uninit();
        let err = unsafe {
            relinow_state_reliable_send(&mut self.inner, peer_index, channel_id, now_ms, result.as_mut_ptr())
        };
        StateError::into_result(err).map(|_| unsafe { result.assume_init() })
    }

    /// Handles an incoming ACK for a RELIABLE channel.
    pub fn reliable_on_ack(
        &mut self,
        peer_index: u8,
        channel_id: u8,
        ack_id: u16,
        now_ms: u32,
    ) -> Result<(), StateError> {
        let err = unsafe {
            relinow_state_reliable_on_ack(&mut self.inner, peer_index, channel_id, ack_id, now_ms)
        };
        StateError::into_result(err)
    }

    /// Polls a RELIABLE channel for retransmissions.
    pub fn reliable_poll(
        &mut self,
        peer_index: u8,
        channel_id: u8,
        now_ms: u32,
    ) -> Result<relinow_reliable_tx_result_t, StateError> {
        let mut result = MaybeUninit::<relinow_reliable_tx_result_t>::uninit();
        let err = unsafe {
            relinow_state_reliable_poll(&mut self.inner, peer_index, channel_id, now_ms, result.as_mut_ptr())
        };
        StateError::into_result(err).map(|_| unsafe { result.assume_init() })
    }

    /// Handles incoming RELIABLE data.
    pub fn reliable_on_data(
        &mut self,
        peer_index: u8,
        channel_id: u8,
        seq_id: u16,
    ) -> Result<relinow_reliable_rx_result_t, StateError> {
        let mut result = MaybeUninit::<relinow_reliable_rx_result_t>::uninit();
        let err = unsafe {
            relinow_state_reliable_on_data(&mut self.inner, peer_index, channel_id, seq_id, result.as_mut_ptr())
        };
        StateError::into_result(err).map(|_| unsafe { result.assume_init() })
    }

    /// Gets the current RTT estimate for a RELIABLE channel.
    pub fn reliable_get_rtt_ms(&self, peer_index: u8, channel_id: u8) -> Result<u16, StateError> {
        let mut rtt_ms = 0;
        let err = unsafe {
            relinow_state_reliable_get_rtt_ms(&self.inner, peer_index, channel_id, &mut rtt_ms)
        };
        StateError::into_result(err).map(|_| rtt_ms)
    }

    /// Sends an UNRELIABLE packet. Returns the sequence ID generated.
    pub fn unreliable_send(&mut self, peer_index: u8, channel_id: u8) -> Result<u16, StateError> {
        let mut seq_id = 0;
        let err = unsafe {
            relinow_state_unreliable_send(&mut self.inner, peer_index, channel_id, &mut seq_id)
        };
        StateError::into_result(err).map(|_| seq_id)
    }

    /// Handles incoming UNRELIABLE data.
    pub fn unreliable_on_data(&mut self, peer_index: u8, channel_id: u8, seq_id: u16) -> Result<(), StateError> {
        let err = unsafe {
            relinow_state_unreliable_on_data(&mut self.inner, peer_index, channel_id, seq_id)
        };
        StateError::into_result(err)
    }

    /// Sends a PRIORITY packet. Returns (seq_id, replaced, replaced_seq_id).
    pub fn priority_send(&mut self, peer_index: u8, channel_id: u8) -> Result<(u16, bool, u16), StateError> {
        let mut seq_id = 0;
        let mut replaced = 0;
        let mut replaced_seq = 0;
        let err = unsafe {
            relinow_state_priority_send(
                &mut self.inner,
                peer_index,
                channel_id,
                &mut seq_id,
                &mut replaced,
                &mut replaced_seq,
            )
        };
        StateError::into_result(err).map(|_| (seq_id, replaced != 0, replaced_seq))
    }

    /// Handles incoming PRIORITY data. Returns true if the packet should be delivered.
    pub fn priority_on_data(&mut self, peer_index: u8, channel_id: u8, seq_id: u16) -> Result<bool, StateError> {
        let mut should_deliver = 0;
        let err = unsafe {
            relinow_state_priority_on_data(&mut self.inner, peer_index, channel_id, seq_id, &mut should_deliver)
        };
        StateError::into_result(err).map(|_| should_deliver != 0)
    }

    /// Configures the heartbeat mechanism for a peer.
    pub fn configure_heartbeat(
        &mut self,
        peer_index: u8,
        interval_ms: u16,
        miss_count_max: u8,
    ) -> Result<(), StateError> {
        let err = unsafe {
            relinow_state_configure_heartbeat(
                &mut self.inner,
                peer_index,
                interval_ms,
                miss_count_max,
            )
        };
        StateError::into_result(err)
    }

    /// Polls the heartbeat mechanism. Returns (should_ping, peer_timeout).
    pub fn poll_heartbeat(&mut self, peer_index: u8, now_ms: u32) -> Result<(bool, bool), StateError> {
        let mut should_ping = 0;
        let mut peer_timeout = 0;
        let err = unsafe {
            relinow_state_poll_heartbeat(&mut self.inner, peer_index, now_ms, &mut should_ping, &mut peer_timeout)
        };
        StateError::into_result(err).map(|_| (should_ping != 0, peer_timeout != 0))
    }

    /// Handles an incoming PONG message.
    pub fn on_pong(&mut self, peer_index: u8) -> Result<(), StateError> {
        let err = unsafe {
            relinow_state_on_pong(&mut self.inner, peer_index)
        };
        StateError::into_result(err)
    }

    /// Gets the statistics for a specific channel.
    pub fn get_stats(&self, peer_index: u8, channel_id: u8) -> Result<relinow_stats_t, StateError> {
        let mut stats = MaybeUninit::<relinow_stats_t>::uninit();
        let err = unsafe {
            relinow_state_get_stats(&self.inner, peer_index, channel_id, stats.as_mut_ptr())
        };
        StateError::into_result(err).map(|_| unsafe { stats.assume_init() })
    }

    /// Asks the scheduler for the next channel that needs transmitting.
    pub fn scheduler_next(&mut self, peer_index: u8, now_ms: u32) -> Result<(u8, relinow_reliable_tx_result_t), StateError> {
        let mut channel_id = 0;
        let mut tx_result = MaybeUninit::<relinow_reliable_tx_result_t>::uninit();
        let err = unsafe {
            relinow_state_scheduler_next(&mut self.inner, peer_index, now_ms, &mut channel_id, tx_result.as_mut_ptr())
        };
        StateError::into_result(err).map(|_| unsafe { (channel_id, tx_result.assume_init()) })
    }
}

impl Default for RelinowState {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_state_init_and_add_peer() {
        let mut state = RelinowState::new();
        let mac = [0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF];
        
        let peer_idx = state.add_peer(&mac).expect("Failed to add peer");
        assert_eq!(peer_idx, 0);

        let found_idx = state.find_peer(&mac).expect("Failed to find peer");
        assert_eq!(found_idx, 0);

        let unknown_mac = [0x00, 0x00, 0x00, 0x00, 0x00, 0x00];
        assert_eq!(state.find_peer(&unknown_mac), Err(StateError::NotFound));
    }

    #[test]
    fn test_channel_operations() {
        let mut state = RelinowState::new();
        let mac = [0x11, 0x22, 0x33, 0x44, 0x55, 0x66];
        let peer_idx = state.add_peer(&mac).unwrap();

        state.open_channel(peer_idx, 10, RELINOW_MODE_RELIABLE, 5).unwrap();
        
        let mode = state.get_channel_mode(peer_idx, 10).unwrap();
        assert_eq!(mode, RELINOW_MODE_RELIABLE);

        let cfg = crate::reliable::default_reliable_config();
        state.set_reliable_config(peer_idx, 10, &cfg).unwrap();

        let tx = state.reliable_send(peer_idx, 10, 1000).unwrap();
        assert_eq!(tx.event, RELINOW_RELIABLE_TX_NEW);
    }
}
