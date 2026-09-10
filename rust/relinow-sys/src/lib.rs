#![no_std]
#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

pub type size_t = usize;

/* -------------------------------------------------------------------------- */
/* relinow_packet.h                                                           */
/* -------------------------------------------------------------------------- */

pub const RELINOW_HEADER_SIZE: u8 = 9;
pub const RELINOW_PROTOCOL_VERSION: u8 = 0x01;

pub const RELINOW_MODE_RELIABLE: u8 = 0x01;
pub const RELINOW_MODE_UNRELIABLE: u8 = 0x02;
pub const RELINOW_MODE_PRIORITY: u8 = 0x03;

pub const RELINOW_TYPE_DATA: u8 = 0x01;
pub const RELINOW_TYPE_ACK: u8 = 0x02;
pub const RELINOW_TYPE_NACK: u8 = 0x03;
pub const RELINOW_TYPE_PING: u8 = 0x04;
pub const RELINOW_TYPE_PONG: u8 = 0x05;

pub const RELINOW_FLAG_FRAGMENT: u8 = 0x01;
pub const RELINOW_FLAG_LAST_FRAGMENT: u8 = 0x02;
pub const RELINOW_FLAG_ENCRYPTED: u8 = 0x04;
pub const RELINOW_FLAG_RESERVED: u8 = 0x08;

pub const RELINOW_HEARTBEAT_CHANNEL: u8 = 0xFF;

pub type relinow_err_t = i32;
pub const RELINOW_ERR_OK: relinow_err_t = 0;
pub const RELINOW_ERR_INVALID_ARG: relinow_err_t = 1;
pub const RELINOW_ERR_INVALID_PACKET: relinow_err_t = 2;
pub const RELINOW_ERR_PAYLOAD_TOO_LARGE: relinow_err_t = 3;

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct relinow_header_t {
    pub version: u8,
    pub mode: u8,
    pub type_: u8,
    pub flags: u8,
    pub seq_id: u16,
    pub ack_id: u16,
    pub channel_id: u8,
    pub payload_len: u16,
}

extern "C" {
    pub fn relinow_encode_header(
        header: *const relinow_header_t,
        max_payload: u16,
        out_bytes: *mut u8,
    ) -> relinow_err_t;

    pub fn relinow_decode_header(
        frame: *const u8,
        frame_len: size_t,
        max_payload: u16,
        out_header: *mut relinow_header_t,
    ) -> relinow_err_t;

    pub fn relinow_validate_header(
        header: *const relinow_header_t,
        max_payload: u16,
    ) -> relinow_err_t;

    pub fn relinow_is_seq_newer(seq_a: u16, seq_b: u16) -> core::ffi::c_int;
    pub fn relinow_seq_next(current: u16) -> u16;
}

/* -------------------------------------------------------------------------- */
/* relinow_reliable.h                                                         */
/* -------------------------------------------------------------------------- */

pub const RELINOW_RELIABLE_MAX_PENDING: usize = 16;
pub const RELINOW_RELIABLE_MAX_REORDER: usize = 8;

pub type relinow_reliable_tx_event_t = u32;
pub const RELINOW_RELIABLE_TX_NONE: relinow_reliable_tx_event_t = 0;
pub const RELINOW_RELIABLE_TX_NEW: relinow_reliable_tx_event_t = 1;
pub const RELINOW_RELIABLE_TX_RETRANSMIT: relinow_reliable_tx_event_t = 2;
pub const RELINOW_RELIABLE_TX_FAILED: relinow_reliable_tx_event_t = 3;

pub type relinow_reliable_rx_status_t = u32;
pub const RELINOW_RELIABLE_RX_DELIVERED: relinow_reliable_rx_status_t = 0;
pub const RELINOW_RELIABLE_RX_OUT_OF_ORDER: relinow_reliable_rx_status_t = 1;
pub const RELINOW_RELIABLE_RX_DUPLICATE: relinow_reliable_rx_status_t = 2;
pub const RELINOW_RELIABLE_RX_DROPPED: relinow_reliable_rx_status_t = 3;

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct relinow_reliable_config_t {
    pub max_retries: u8,
    pub initial_rtt_ms: u16,
    pub rtt_alpha_q8: u8,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct relinow_reliable_tx_result_t {
    pub event: relinow_reliable_tx_event_t,
    pub seq_id: u16,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct relinow_reliable_rx_result_t {
    pub status: relinow_reliable_rx_status_t,
    pub delivered_count: u8,
    pub delivered_seq: [u16; 1 + RELINOW_RELIABLE_MAX_REORDER],
    pub ack_id: u16,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct relinow_reliable_pending_t {
    pub in_use: u8,
    pub seq_id: u16,
    pub last_send_ms: u32,
    pub timeout_ms: u16,
    pub retries: u8,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct relinow_reliable_reorder_t {
    pub in_use: u8,
    pub seq_id: u16,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct relinow_reliable_ctx_t {
    pub cfg: relinow_reliable_config_t,
    pub next_tx_seq: u16,
    pub rtt_estimate_ms: u16,
    pub has_rx_window: u8,
    pub expected_rx_seq: u16,
    pub pending: [relinow_reliable_pending_t; RELINOW_RELIABLE_MAX_PENDING],
    pub reorder: [relinow_reliable_reorder_t; RELINOW_RELIABLE_MAX_REORDER],
}

extern "C" {
    pub fn relinow_reliable_default_config(out_cfg: *mut relinow_reliable_config_t);
    pub fn relinow_reliable_init(
        ctx: *mut relinow_reliable_ctx_t,
        cfg: *const relinow_reliable_config_t,
    );

    pub fn relinow_reliable_send(
        ctx: *mut relinow_reliable_ctx_t,
        now_ms: u32,
        out_result: *mut relinow_reliable_tx_result_t,
    ) -> relinow_err_t;

    pub fn relinow_reliable_on_ack(
        ctx: *mut relinow_reliable_ctx_t,
        ack_id: u16,
        now_ms: u32,
    ) -> relinow_reliable_tx_event_t;

    pub fn relinow_reliable_poll(
        ctx: *mut relinow_reliable_ctx_t,
        now_ms: u32,
        out_result: *mut relinow_reliable_tx_result_t,
    ) -> relinow_err_t;

    pub fn relinow_reliable_on_data(
        ctx: *mut relinow_reliable_ctx_t,
        seq_id: u16,
        out_result: *mut relinow_reliable_rx_result_t,
    ) -> relinow_err_t;

    pub fn relinow_reliable_current_rtt_ms(ctx: *const relinow_reliable_ctx_t) -> u16;
    pub fn relinow_reliable_compute_timeout_ms(
        ctx: *const relinow_reliable_ctx_t,
        retry_index: u8,
    ) -> u16;
}

/* -------------------------------------------------------------------------- */
/* relinow_state.h                                                            */
/* -------------------------------------------------------------------------- */

pub const RELINOW_MAX_PEERS: usize = 20;
pub const RELINOW_MAX_CHANNELS_PER_PEER: usize = 16;
pub const RELINOW_TX_QUEUE_SIZE: usize = 16;
pub const RELINOW_RX_QUEUE_SIZE: usize = 8;

pub type relinow_state_err_t = u32;
pub const RELINOW_STATE_OK: relinow_state_err_t = 0;
pub const RELINOW_STATE_ERR_INVALID_ARG: relinow_state_err_t = 1;
pub const RELINOW_STATE_ERR_NOT_FOUND: relinow_state_err_t = 2;
pub const RELINOW_STATE_ERR_NO_SPACE: relinow_state_err_t = 3;
pub const RELINOW_STATE_ERR_CONFLICT: relinow_state_err_t = 4;
pub const RELINOW_STATE_ERR_QUEUE_FULL: relinow_state_err_t = 5;
pub const RELINOW_STATE_ERR_WRONG_MODE: relinow_state_err_t = 6;

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct relinow_tx_slot_t {
    pub used: u8,
    pub seq_id: u16,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct relinow_rx_slot_t {
    pub used: u8,
    pub seq_id: u16,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct relinow_stats_t {
    pub total_rx: u32,
    pub total_lost: u32,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct relinow_channel_state_t {
    pub in_use: u8,
    pub channel_id: u8,
    pub mode: u8,
    pub priority: u8,
    pub next_tx_seq: u16,
    pub expected_rx_seq: u16,
    pub has_rx_seq: u8,
    pub has_inflight: u8,
    pub inflight_seq: u16,
    pub stats: relinow_stats_t,
    pub reliable: relinow_reliable_ctx_t,
    pub tx_queue: [relinow_tx_slot_t; RELINOW_TX_QUEUE_SIZE],
    pub rx_queue: [relinow_rx_slot_t; RELINOW_RX_QUEUE_SIZE],
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct relinow_peer_state_t {
    pub in_use: u8,
    pub mac: [u8; 6],
    pub heartbeat_interval_ms: u16,
    pub heartbeat_miss_count_max: u8,
    pub last_ping_sent_ms: u32,
    pub missed_pongs: u8,
    pub last_polled_channel_idx: u8,
    pub channels: [relinow_channel_state_t; RELINOW_MAX_CHANNELS_PER_PEER],
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct relinow_state_t {
    pub peers: [relinow_peer_state_t; RELINOW_MAX_PEERS],
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct relinow_state_limits_t {
    pub max_peers: u16,
    pub max_channels_per_peer: u16,
    pub tx_queue_size: u16,
    pub rx_queue_size: u16,
    pub state_footprint_bytes: size_t,
}

extern "C" {
    pub fn relinow_state_init(state: *mut relinow_state_t);

    pub fn relinow_state_get_limits(out_limits: *mut relinow_state_limits_t);

    pub fn relinow_state_add_peer(
        state: *mut relinow_state_t,
        mac: *const u8,
        out_peer_index: *mut u8,
    ) -> relinow_state_err_t;

    pub fn relinow_state_find_peer(
        state: *const relinow_state_t,
        mac: *const u8,
        out_peer_index: *mut u8,
    ) -> relinow_state_err_t;

    pub fn relinow_state_open_channel(
        state: *mut relinow_state_t,
        peer_index: u8,
        channel_id: u8,
        mode: u8,
        priority: u8,
    ) -> relinow_state_err_t;

    pub fn relinow_state_next_sequence(
        state: *mut relinow_state_t,
        peer_index: u8,
        channel_id: u8,
        out_seq: *mut u16,
    ) -> relinow_state_err_t;

    pub fn relinow_state_get_channel_mode(
        state: *const relinow_state_t,
        peer_index: u8,
        channel_id: u8,
        out_mode: *mut u8,
    ) -> relinow_state_err_t;

    pub fn relinow_state_mark_inflight(
        state: *mut relinow_state_t,
        peer_index: u8,
        channel_id: u8,
        seq_id: u16,
    ) -> relinow_state_err_t;

    pub fn relinow_state_clear_inflight(
        state: *mut relinow_state_t,
        peer_index: u8,
        channel_id: u8,
        seq_id: u16,
    ) -> relinow_state_err_t;

    pub fn relinow_state_enqueue_tx(
        state: *mut relinow_state_t,
        peer_index: u8,
        channel_id: u8,
        seq_id: u16,
    ) -> relinow_state_err_t;

    pub fn relinow_state_enqueue_rx(
        state: *mut relinow_state_t,
        peer_index: u8,
        channel_id: u8,
        seq_id: u16,
    ) -> relinow_state_err_t;

    pub fn relinow_state_set_reliable_config(
        state: *mut relinow_state_t,
        peer_index: u8,
        channel_id: u8,
        cfg: *const relinow_reliable_config_t,
    ) -> relinow_state_err_t;

    pub fn relinow_state_reliable_send(
        state: *mut relinow_state_t,
        peer_index: u8,
        channel_id: u8,
        now_ms: u32,
        out_result: *mut relinow_reliable_tx_result_t,
    ) -> relinow_state_err_t;

    pub fn relinow_state_reliable_on_ack(
        state: *mut relinow_state_t,
        peer_index: u8,
        channel_id: u8,
        ack_id: u16,
        now_ms: u32,
    ) -> relinow_state_err_t;

    pub fn relinow_state_reliable_poll(
        state: *mut relinow_state_t,
        peer_index: u8,
        channel_id: u8,
        now_ms: u32,
        out_result: *mut relinow_reliable_tx_result_t,
    ) -> relinow_state_err_t;

    pub fn relinow_state_reliable_on_data(
        state: *mut relinow_state_t,
        peer_index: u8,
        channel_id: u8,
        seq_id: u16,
        out_result: *mut relinow_reliable_rx_result_t,
    ) -> relinow_state_err_t;

    pub fn relinow_state_reliable_get_rtt_ms(
        state: *const relinow_state_t,
        peer_index: u8,
        channel_id: u8,
        out_rtt_ms: *mut u16,
    ) -> relinow_state_err_t;

    pub fn relinow_state_unreliable_send(
        state: *mut relinow_state_t,
        peer_index: u8,
        channel_id: u8,
        out_seq_id: *mut u16,
    ) -> relinow_state_err_t;

    pub fn relinow_state_unreliable_on_data(
        state: *mut relinow_state_t,
        peer_index: u8,
        channel_id: u8,
        seq_id: u16,
    ) -> relinow_state_err_t;

    pub fn relinow_state_priority_send(
        state: *mut relinow_state_t,
        peer_index: u8,
        channel_id: u8,
        out_seq_id: *mut u16,
        out_replaced: *mut u8,
        out_replaced_seq_id: *mut u16,
    ) -> relinow_state_err_t;

    pub fn relinow_state_priority_on_data(
        state: *mut relinow_state_t,
        peer_index: u8,
        channel_id: u8,
        seq_id: u16,
        out_should_deliver: *mut u8,
    ) -> relinow_state_err_t;

    pub fn relinow_state_clear_inflight_any(
        state: *mut relinow_state_t,
        peer_index: u8,
        channel_id: u8,
    ) -> relinow_state_err_t;

    pub fn relinow_state_configure_heartbeat(
        state: *mut relinow_state_t,
        peer_index: u8,
        interval_ms: u16,
        miss_count_max: u8,
    ) -> relinow_state_err_t;

    pub fn relinow_state_poll_heartbeat(
        state: *mut relinow_state_t,
        peer_index: u8,
        now_ms: u32,
        out_should_ping: *mut u8,
        out_peer_timeout: *mut u8,
    ) -> relinow_state_err_t;

    pub fn relinow_state_on_pong(
        state: *mut relinow_state_t,
        peer_index: u8,
    ) -> relinow_state_err_t;

    pub fn relinow_state_get_stats(
        state: *const relinow_state_t,
        peer_index: u8,
        channel_id: u8,
        out_stats: *mut relinow_stats_t,
    ) -> relinow_state_err_t;

    pub fn relinow_state_scheduler_next(
        state: *mut relinow_state_t,
        peer_index: u8,
        now_ms: u32,
        out_channel_id: *mut u8,
        out_tx: *mut relinow_reliable_tx_result_t,
    ) -> relinow_state_err_t;
}
