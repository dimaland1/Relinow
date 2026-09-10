# Multiplexing & Heartbeat

ReliNow natively multiplexes multiple logical streams over a single wireless connection and provides built-in link liveness monitoring.

---

## 1. Virtual Channels (0 to 15)

In raw ESP-NOW, all frames land in a single global callback. Applications that mix sensor data, shell commands, and status LEDs must manually demultiplex them.

ReliNow integrates **16 independent virtual channels** per peer:
- Each channel possesses its own independent sequence numbers (`next_tx_seq`, `expected_rx_seq`), transmission mode (RELIABLE, UNRELIABLE, or PRIORITY), and flow control window.
- Channel state isolation ensures that a stall on a bulk transfer channel (e.g. Channel 1, RELIABLE) never blocks high-priority control inputs (e.g. Channel 2, PRIORITY).

### Channel Scheduler
When multiple channels have frames waiting to transmit, `relinow_state_scheduler_next` schedules them fairly:
1. **Pass 1 (Retransmissions)**: Urgent expired frames requiring retry take precedence.
2. **Pass 2 (Priority)**: Channels configured in PRIORITY mode are served next.
3. **Pass 3 (Round-Robin)**: Remaining RELIABLE and UNRELIABLE traffic is served round-robin to prevent channel starvation.

---

## 2. Heartbeat & Liveness Subsystem (Channel `0xFF`)

ReliNow reserves Channel `0xFF` for link liveness monitoring:
- **Periodic PING**: When `heartbeat_interval_ms > 0`, the transmitter automatically emits a lightweight PING frame.
- **Immediate PONG**: Receivers intercept Channel `0xFF` frames and immediately return an acknowledgment PONG frame.
- **Dead-Node Detection**: If consecutive PONG responses fail to arrive and exceed `heartbeat_miss_count_max`, ReliNow triggers the `on_peer_timeout` callback, allowing the application to enter failsafe mode or trigger peer rediscovery.
