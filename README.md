# ReliNow

A lightweight reliable transport protocol built on top of [ESP-NOW](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_now.html).

## The Problem

ESP-NOW is fast, low-power, and connectionless. But it provides no reliability guarantees at the application layer. Every developer who needs acknowledgments, ordering, or retransmission ends up writing their own ad-hoc solution from scratch.

ReliNow solves this by providing a reusable transport layer with three transmission modes, designed for the constraints of embedded systems.

## Three Modes

**RELIABLE** — Acknowledged, ordered, retransmitted on loss. For commands that must arrive.

**UNRELIABLE** — Fire and forget. Sequence numbers track loss, but no retransmission. For high-frequency data where the next packet replaces the last.

**PRIORITY** — Newest-wins. Only the latest packet matters. Stale unacknowledged packets are discarded. For real-time control where latency beats completeness.

## Design

- 9-byte header, 241 bytes of usable payload per ESP-NOW frame
- Per-peer adaptive RTT estimation (EWMA, same approach as TCP)
- Channel multiplexing: multiple logical streams over a single link
- Fragmentation for large messages in RELIABLE mode
- Heartbeat for connection liveness detection
- Zero dynamic allocation, suitable for `no_std` / bare-metal

Full protocol specification: [PROTOCOL.md](PROTOCOL.md)

## Architecture

```
┌─────────────────────────────┐
│     Application             |
├─────────────────────────────┤
│     ReliNow API             │
├─────────────────────────────┤
│     ReliNow Core            │
├─────────────────────────────┤
│     ESP-NOW (Espressif)     │
├─────────────────────────────┤
│     WiFi 802.11 (MAC)       │
└─────────────────────────────┘
```

## Project Structure

```
relinow/
├── README.md
├── LICENSE
├── docs/
│   └── PROTOCOL.md
├── core/                    ← C library (ESP-IDF component)
│   ├── CMakeLists.txt
│   ├── include/
│   │   └── relinow.h
│   └── src/
│       ├── packet.c
│       ├── peer.c
│       ├── reliable.c
│       ├── unreliable.c
│       ├── priority.c
│       ├── channel.c
│       ├── fragment.c
│       ├── rtt.c
│       └── relinow.c
├── rust/                    ← Rust wrapper (crate)
│   ├── Cargo.toml
│   └── src/
│       └── lib.rs
├── examples/
│   ├── c/
│   └── rust/
└── tests/
```

## Roadmap

- [x] Protocol specification
- [ ] Packet serialization / deserialization
- [ ] PRIORITY mode (newest-wins)
- [ ] UNRELIABLE mode (fire-and-forget with loss stats)
- [ ] Channel multiplexing and scheduler
- [ ] RELIABLE mode with ordering and retransmission
- [ ] Fragmentation / reassembly
- [ ] Rust wrapper
- [ ] Benchmarks and documentation

## Hardware

Tested on:
- ESP32-WROOM
- ESP32-C3

## Status

**Work in progress.** Protocol spec is complete. Implementation starting.

## License

MIT
