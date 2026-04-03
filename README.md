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
├── PROTOCOL.md
├── core/                    ← C library + ESP-IDF transport adapter
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── relinow_packet.h
│   │   ├── relinow_state.h
│   │   ├── relinow_reliable.h
│   │   └── relinow_espnow.h
│   └── src/
│       ├── relinow_packet.c
│       ├── relinow_state.c
│       ├── relinow_reliable.c
│       └── relinow_espnow.c
├── examples/
│   ├── esp32_a/             ← board A example (ESP-IDF)
│   ├── esp32_b/             ← board B example (ESP-IDF)
│   └── README.md
├── tests/
│   ├── CMakeLists.txt
│   ├── include/
│   ├── src/
│   ├── fixtures/
│   └── README.md
├── tools/
│   ├── build_fixtures.py
│   └── requirements.txt
├── TEST_MATRIX.md
├── TEST_MATRIX.yaml
└── TEST_VECTORS.yaml
```

## Run Tests from Command Line

You can generate fixtures, build tests, and run the full suite directly from a terminal.

Prerequisites:

- Python 3.10+
- CMake 3.16+
- A C compiler toolchain (MSVC, clang, or gcc)

From the repository root:

```powershell
python -m pip install -r tools/requirements.txt
python tools/build_fixtures.py --matrix-yaml TEST_MATRIX.yaml --vector-yaml TEST_VECTORS.yaml --out-dir tests/fixtures
cmake -S tests -B build/tests
cmake --build build/tests
ctest --test-dir build/tests -C Debug --output-on-failure
```

Notes:

- On Windows with Visual Studio generators, keep `-C Debug` in the `ctest` command.
- On single-config generators (Ninja/Unix Makefiles), `ctest --test-dir build/tests --output-on-failure` is usually enough.
- Additional test details are available in `tests/README.md`.

## Test on Two ESP32 Boards (ESP-NOW)

Two ready-to-flash ESP-IDF examples are available:

- `examples/esp32_a`
- `examples/esp32_b`

They connect ReliNow RELIABLE flow to real ESP-NOW callbacks (`send_cb`, `recv_cb`) and run retransmission polling in the main loop.

Quick start:

```bash
cd examples/esp32_a
idf.py set-target esp32
idf.py -p <PORT_A> flash monitor
```

```bash
cd examples/esp32_b
idf.py set-target esp32
idf.py -p <PORT_B> flash monitor
```

Before final test, set each board peer MAC in:

- `examples/esp32_a/main/main.c` (`PEER_MAC` = board B MAC)
- `examples/esp32_b/main/main.c` (`PEER_MAC` = board A MAC)

Full hardware instructions are in `examples/README.md`.

## Roadmap

- [x] Protocol specification
- [x] Packet serialization / deserialization
- [x] RELIABLE MVP (DATA + ACK + retransmission + ordering baseline)
- [x] RTT adaptive timeout + retry backoff
- [ ] PRIORITY mode (newest-wins)
- [ ] UNRELIABLE mode (fire-and-forget with loss stats)
- [ ] Channel multiplexing and scheduler
- [ ] Fragmentation / reassembly
- [ ] Rust wrapper
- [ ] Benchmarks and documentation

## Hardware

Tested on:
- ESP32-WROOM-32
- ESP32-C3
- ESP32-S3

## Status

**Work in progress.** Protocol spec is complete, RELIABLE MVP and ESP-NOW A/B examples are implemented.

## License

MIT
