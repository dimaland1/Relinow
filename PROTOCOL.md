# ReliNow Protocol Specification v1.0

## Overview

ReliNow is a lightweight transport layer built on top of ESP-NOW that provides three transmission modes (reliable, unreliable, priority) with minimal overhead. It is protocol-agnostic and application-agnostic.

ESP-NOW provides a raw connectionless data link layer limited to 250 bytes per frame. ReliNow adds sequencing, acknowledgment, retransmission, multiplexing, and fragmentation without requiring a full TCP/IP stack.

## Design Constraints

| Constraint | Value |
|---|---|
| Max ESP-NOW frame | 250 bytes |
| ReliNow header | 9 bytes |
| Max payload per frame (v1) | 241 bytes |
| Max payload per frame (v2) | 1461 bytes |
| Max peers (ESP-NOW limit) | 20 (unencrypted) / 6 (encrypted) |
| Max channels per peer | 255 (0xFF reserved for heartbeat) |
| Sequence ID space | 0–65535 (wraps around) |

## Packet Format

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Ver  | Mode  |  Type | Flags |         Sequence ID           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           Ack ID              |  Channel ID   |  Payload Len
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  (cont.)       |           Payload ...                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

Total header: 9 bytes.

### Byte Layout

| Offset | Size | Field |
|---|---|---|
| 0 | 4 bits | Version |
| 0 | 4 bits | Mode |
| 1 | 4 bits | Type |
| 1 | 4 bits | Flags |
| 2–3 | 16 bits | Sequence ID (big-endian) |
| 4–5 | 16 bits | Ack ID (big-endian) |
| 6 | 8 bits | Channel ID |
| 7–8 | 16 bits | Payload Length (big-endian) |
| 9+ | variable | Payload (max 241 bytes on ESP-NOW v1) |

### Field Definitions

#### Version (4 bits)

Protocol version. Current value: `0x01`.

Receivers MUST silently discard packets with an unknown version.

#### Mode (4 bits)

| Value | Name | Description |
|---|---|---|
| 0x01 | RELIABLE | Acknowledged, ordered, retransmitted on loss |
| 0x02 | UNRELIABLE | No acknowledgment, no retransmission |
| 0x03 | PRIORITY | Acknowledged, newest-wins, no retransmission of stale packets |

#### Type (4 bits)

| Value | Name | Description |
|---|---|---|
| 0x01 | DATA | Application data |
| 0x02 | ACK | Acknowledgment of received DATA |
| 0x03 | NACK | Negative acknowledgment (receiver detected gap) |
| 0x04 | PING | Heartbeat request |
| 0x05 | PONG | Heartbeat response |

#### Flags (4 bits)

Bit flags, can be combined:

| Bit | Name | Description |
|---|---|---|
| 0 | FRAGMENT | This packet is part of a fragmented message |
| 1 | LAST_FRAGMENT | This is the last fragment of a fragmented message |
| 2 | ENCRYPTED | Payload is encrypted at the ReliNow layer |
| 3 | Reserved | Must be 0 |

#### Sequence ID (16 bits, unsigned, big-endian)

Monotonically increasing per (peer, channel) pair. Wraps around from 65535 to 0.

Used by the sender to identify each packet uniquely within a channel.

#### Ack ID (16 bits, unsigned, big-endian)

In ACK packets: the Sequence ID being acknowledged.

In DATA packets: piggybacked ACK of the last received sequence from the other side (reduces ACK-only traffic).

In UNRELIABLE mode: set to 0, ignored by receiver.

#### Channel ID (8 bits, unsigned)

Logical channel for multiplexing multiple streams over a single ESP-NOW link. Valid range: 0–254. Channel 0xFF is reserved for heartbeat (PING/PONG).

Each channel has:
- Its own transmission mode (set at channel open time, immutable)
- Its own sequence counter
- Its own retransmission queue
- A configurable priority level for scheduling

**Design note:** 8 bits (255 usable channels) is sufficient given the ESP-NOW limit of 20 peers. Keeping this field to 1 byte minimizes header overhead on a 250-byte frame.

#### Payload Length (16 bits, unsigned, big-endian)

Length of the payload in bytes. Maximum value: 241 (ESP-NOW v1) or 1461 (ESP-NOW v2).

A value of 0 is valid (e.g., for ACK, PING, PONG packets with no payload).

## Transmission Modes

### RELIABLE Mode

Provides ordered, acknowledged delivery with retransmission.

#### Sender Behavior

1. Assign next Sequence ID for (peer, channel).
2. Enqueue packet in the retransmission buffer.
3. Transmit via ESP-NOW.
4. Start retransmission timer: `timeout = rtt_estimate × 1.5`.
5. On ACK received: remove packet from retransmission buffer.
6. On timeout: retransmit. Increment retry counter. Apply backoff.
7. After `max_retries` (default: 3): report failure via callback. Remove from buffer.

#### Receiver Behavior

1. On DATA received: check Sequence ID.
2. If `seq == expected_seq`: deliver to application, increment `expected_seq`, send ACK.
3. If `seq < expected_seq`: duplicate, silently discard, resend ACK (sender may have missed it).
4. If `seq > expected_seq`: out of order, buffer for later delivery, optionally send NACK for the gap.

#### Ordering Guarantee

Packets are delivered to the application in Sequence ID order. Out-of-order packets are buffered until the gap is filled or a timeout expires.

#### Duplicate Detection

Receiver tracks `expected_seq` per (peer, channel). Any packet with `seq < expected_seq` (using serial arithmetic) is a duplicate and is discarded.

### UNRELIABLE Mode

Fire-and-forget transmission with optional loss detection.

#### Sender Behavior

1. Assign next Sequence ID for (peer, channel).
2. Transmit via ESP-NOW.
3. No retransmission buffer, no timer, no ACK expected.

#### Receiver Behavior

1. On DATA received: deliver to application immediately.
2. Track received Sequence IDs to compute packet loss rate.
3. No ordering guarantee: packets are delivered as they arrive.

#### Loss Detection

The receiver can detect gaps in Sequence IDs:
- Received: 1, 2, 4 → packet 3 was lost.
- This information is exposed via `relinow_get_stats()`.
- The receiver does NOT request retransmission.

### PRIORITY Mode

Newest-wins transmission for real-time control data.

#### Sender Behavior

1. Assign next Sequence ID for (peer, channel).
2. If a previous packet for this (peer, channel) is still unacknowledged: discard it from the retransmission buffer. Do NOT retransmit it.
3. Only one packet in-flight per (peer, channel) at any time.
4. Transmit via ESP-NOW.
5. Start ACK timer.
6. On ACK received: confirm delivery.
7. On timeout: do NOT retransmit the old packet. The next `send()` call will supersede it.

#### Receiver Behavior

1. On DATA received: check Sequence ID.
2. If `seq > last_received_seq` (serial arithmetic): deliver to application, update `last_received_seq`, send ACK.
3. If `seq <= last_received_seq`: stale packet, silently discard.

#### Connection Loss Detection

If `max_inflight_loss` consecutive packets receive no ACK (default: 5), the sender reports connection loss via `on_peer_timeout` callback.

## RTT Estimation

ReliNow maintains a **per-peer** RTT estimate using exponential weighted moving average:

```
rtt_estimate = (1 - alpha) × rtt_estimate + alpha × rtt_sample
```

Where:
- `alpha = 0.125` (same as TCP)
- `rtt_sample` = time between sending DATA and receiving ACK
- Initial `rtt_estimate` = 50ms

The retransmission timeout is computed as:

```
timeout = rtt_estimate × multiplier
```

Where `multiplier` defaults to 1.5 and increases with each retry:
- Retry 1: `rtt_estimate × 1.5`
- Retry 2: `rtt_estimate × 2.0`
- Retry 3: `rtt_estimate × 3.0`

This adaptive backoff prevents flooding a congested link.

**Design note:** RTT is estimated per-peer, not per-channel. Since all channels between two peers share the same physical ESP-NOW link, the RTT reflects the radio link quality rather than per-channel load. Per-channel RTT may be considered in a future version if channel-specific congestion control is added.

## Fragmentation

When a message exceeds the max payload size (241 bytes on ESP-NOW v1), ReliNow splits it into fragments.

### Rules

- Fragmentation is ONLY available in RELIABLE mode.
- UNRELIABLE and PRIORITY modes MUST NOT exceed the max payload size per message. The sender MUST return `RELINOW_ERR_PAYLOAD_TOO_LARGE` if the payload exceeds the limit.
- Each fragment is a separate packet with its own Sequence ID.
- The `FRAGMENT` flag is set on all fragments.
- The `LAST_FRAGMENT` flag is set on the final fragment.
- All fragments share the same Channel ID.

### Single Fragmented Message Per Channel

**Only one fragmented message may be in-flight per channel at any time.** The sender MUST NOT begin transmitting fragments of a new message until all fragments of the previous message have been acknowledged.

This constraint is implicit in RELIABLE mode's ordering guarantee: since packets are delivered in Sequence ID order, and each fragment has its own Sequence ID, the receiver will naturally reassemble fragments sequentially. However, the sender MUST enforce this constraint to prevent interleaving of fragments from different messages.

### Reassembly

The receiver buffers fragments as they arrive. When the fragment with the `LAST_FRAGMENT` flag is received and all preceding fragments in the sequence are present, the complete message is delivered to the application.

If a fragment is lost, the RELIABLE retransmission mechanism recovers it.

### Fragment Timeout

If all fragments are not received within `fragment_timeout` (default: 5 seconds), the partial message is discarded and the sender is NACKed for the missing fragments.

## Channel Multiplexing

Channels allow multiple logical streams over a single ESP-NOW link.

### Properties

Each channel has:
- A unique Channel ID (0–254, 0xFF reserved for heartbeat)
- A transmission mode (RELIABLE, UNRELIABLE, or PRIORITY), set at open time, immutable
- An independent sequence counter
- A scheduling priority (0 = highest, 255 = lowest)

### Scheduling

When multiple channels have packets queued for transmission, the scheduler selects based on:

1. Retransmissions pending on RELIABLE channels (highest priority, always)
2. Channels sorted by scheduling priority (lower number = higher priority)
3. Within same priority: round-robin

### Default Channels

No channels are created by default. The application must explicitly open channels via `relinow_open_channel()` before sending data.

## Heartbeat (PING/PONG)

ReliNow supports optional heartbeat for connection liveness detection.

### Behavior

- Either side can send PING at a configurable interval (default: disabled).
- The receiver MUST respond with PONG within one RTT.
- If `heartbeat_miss_count` consecutive PINGs receive no PONG (default: 3), the peer is considered disconnected and the `on_peer_timeout` callback fires.
- PING/PONG packets use Channel ID `0xFF` (reserved, cannot be opened by application).
- PING/PONG have no payload by default but MAY carry diagnostic data (e.g., stats).

## Piggybacked ACKs

To reduce the number of standalone ACK packets, DATA packets carry a piggybacked ACK in the Ack ID field.

When a sender transmits DATA, it sets `ack_id` to the last Sequence ID it received from that peer on the same channel. The receiver processes both the DATA and the piggybacked ACK.

If there is no recent data to send, a standalone ACK packet is sent instead.

Piggybacked ACKs are only meaningful on bidirectional channels. On unidirectional flows, the receiver sends standalone ACKs.

## Error Handling

### Error Codes

| Code | Name | Description |
|---|---|---|
| 0x00 | OK | Success |
| 0x01 | ERR_PEER_NOT_FOUND | Unknown peer MAC address |
| 0x02 | ERR_CHANNEL_NOT_OPEN | Channel not opened |
| 0x03 | ERR_PAYLOAD_TOO_LARGE | Payload exceeds max size for mode |
| 0x04 | ERR_SEND_FAILED | ESP-NOW send failed at MAC layer |
| 0x05 | ERR_TIMEOUT | ACK not received within max retries |
| 0x06 | ERR_PEER_DISCONNECTED | Heartbeat lost |
| 0x07 | ERR_FRAGMENT_TIMEOUT | Incomplete fragmented message |
| 0x08 | ERR_NOT_INITIALIZED | ReliNow not initialized |
| 0x09 | ERR_QUEUE_FULL | Transmission queue is full |
| 0x0A | ERR_INVALID_PACKET | Malformed or unknown version packet |

### Callbacks

All errors are reported asynchronously via registered callbacks:
- `on_send_complete(mac, channel_id, seq_id, status)` — called after send success or final failure
- `on_peer_timeout(mac)` — called when heartbeat is lost or PRIORITY connection loss detected

## Sequence ID Wraparound

Sequence IDs are 16-bit unsigned integers (0–65535).

When comparing sequence numbers for ordering, ReliNow uses serial number arithmetic (RFC 1982):

```
seq_a is "newer" than seq_b if:
  (int16_t)(seq_a - seq_b) > 0
```

This correctly handles wraparound as long as the difference between the oldest unacknowledged packet and the newest packet does not exceed 32767.

## Configuration Defaults

| Parameter | Default | Range | Description |
|---|---|---|---|
| max_retries | 3 | 1–10 | Retransmission attempts in RELIABLE mode |
| initial_rtt_ms | 50 | 10–1000 | Initial RTT estimate before first measurement |
| rtt_alpha | 0.125 | 0.0–1.0 | EWMA smoothing factor for RTT |
| rtt_multiplier | 1.5 | 1.0–5.0 | Base multiplier for retransmission timeout |
| max_inflight_loss | 5 | 1–20 | Consecutive unacked packets before connection loss (PRIORITY) |
| fragment_timeout_ms | 5000 | 1000–30000 | Max time to wait for all fragments |
| heartbeat_interval_ms | 0 (disabled) | 0–60000 | Interval between PING packets. 0 disables heartbeat |
| heartbeat_miss_count | 3 | 1–10 | Missed PINGs before peer timeout |
| tx_queue_size | 16 | 4–64 | Max packets in transmission queue per peer |
| rx_reorder_buffer_size | 8 | 2–32 | Max out-of-order packets buffered in RELIABLE mode |

All values are configurable at initialization via `relinow_config_t`. A macro `RELINOW_DEFAULT_CONFIG()` provides sensible defaults.

## Security Considerations

ReliNow relies on ESP-NOW's built-in CCMP encryption (IEEE 802.11) for link-layer security.

The optional `ENCRYPTED` flag in the header indicates that ReliNow has applied an additional application-layer encryption to the payload. The encryption scheme is not defined by this specification and is left to the application.

ReliNow does NOT provide:
- Authentication beyond ESP-NOW's PMK/LMK mechanism
- Key exchange
- Replay protection beyond duplicate detection via Sequence IDs

Applications requiring stronger security SHOULD implement application-layer encryption and authentication on top of ReliNow.

## Compatibility

| ESP-NOW Version | Max Frame | ReliNow Header | Max Payload |
|---|---|---|---|
| v1.0 | 250 bytes | 9 bytes | 241 bytes |
| v2.0 | 1470 bytes | 9 bytes | 1461 bytes |

ReliNow auto-detects the ESP-NOW version at initialization and adjusts the max payload size accordingly. All header fields remain identical across versions.

## Wire Example

### RELIABLE DATA packet

Sending "HELLO" (5 bytes) on channel 1, sequence 42, piggybacked ack 10:

```
Byte 0:  0x11  → version=1, mode=RELIABLE
Byte 1:  0x10  → type=DATA, flags=NONE
Byte 2:  0x00  → seq_id high byte
Byte 3:  0x2A  → seq_id low byte (42)
Byte 4:  0x00  → ack_id high byte
Byte 5:  0x0A  → ack_id low byte (10)
Byte 6:  0x01  → channel_id (1)
Byte 7:  0x00  → payload_len high byte
Byte 8:  0x05  → payload_len low byte (5)
Byte 9:  0x48  → 'H'
Byte 10: 0x45  → 'E'
Byte 11: 0x4C  → 'L'
Byte 12: 0x4C  → 'L'
Byte 13: 0x4F  → 'O'

Total: 14 bytes on the wire
```

### ACK packet

Acknowledging sequence 42 on channel 1:

```
Byte 0:  0x11  → version=1, mode=RELIABLE
Byte 1:  0x20  → type=ACK, flags=NONE
Byte 2:  0x00  → seq_id high byte (0, unused in ACK)
Byte 3:  0x00  → seq_id low byte
Byte 4:  0x00  → ack_id high byte
Byte 5:  0x2A  → ack_id low byte (42)
Byte 6:  0x01  → channel_id (1)
Byte 7:  0x00  → payload_len high byte
Byte 8:  0x00  → payload_len low byte (0)

Total: 9 bytes on the wire
```

## Future Considerations (not in v1.0)

- Selective ACK bitmask (ACK multiple packets in one response)
- Congestion window (limit in-flight packets dynamically)
- Multi-hop relay (mesh networking)
- CRC checksum in header (currently relies on ESP-NOW's 802.11 FCS)
- Stream-level flow control
- Per-channel RTT estimation
