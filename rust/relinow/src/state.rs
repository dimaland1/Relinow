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
        
        // Add peer
        let peer_idx = state.add_peer(&mac).expect("Failed to add peer");
        assert_eq!(peer_idx, 0);

        // Find peer
        let found_idx = state.find_peer(&mac).expect("Failed to find peer");
        assert_eq!(found_idx, 0);

        // Not found
        let unknown_mac = [0x00, 0x00, 0x00, 0x00, 0x00, 0x00];
        assert_eq!(state.find_peer(&unknown_mac), Err(StateError::NotFound));
    }
}
