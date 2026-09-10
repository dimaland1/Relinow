# ReliNow

<p align="center">
  <img src="https://img.shields.io/badge/License-MIT-blue.svg" alt="License: MIT" />
  <img src="https://img.shields.io/badge/Language-C99-00599C.svg" alt="Language: C99" />
  <img src="https://img.shields.io/badge/Rust-%23!%5Bno__std%5D-DEA584.svg" alt="Rust no_std" />
  <img src="https://img.shields.io/badge/Target-ESP32%20%2F%20ESP--IDF%20v5.2-E7352C.svg" alt="ESP-IDF" />
  <img src="https://img.shields.io/badge/Tests-100%25%20Passing-brightgreen.svg" alt="Tests" />
  <img src="https://img.shields.io/badge/Docs-mdBook-blueviolet.svg" alt="Docs: mdBook" />
</p>

A lightweight, zero-allocation reliable transport protocol built on top of [ESP-NOW](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_now.html) for embedded IoT systems.

---

## The Problem

**ESP-NOW** is blazingly fast (< 2ms latency), connectionless, and low-power. However, at the application layer:
- **No Delivery Guarantees**: Frames dropped due to 2.4 GHz interference, range attenuation, or packet collisions are permanently lost.
- **Strict Payload Cap**: Limited to 250 bytes per physical frame.
- **No Channel Isolation**: All traffic lands in a single callback, making it difficult to cleanly separate telemetry, control signals, and OTA updates.

**ReliNow** solves this by providing a modular transport layer offering TCP-like reliability, sliding window flow control, packet fragmentation, and channel multiplexing—with **zero dynamic heap allocation (`malloc`)**.

---

## Three Transmission Modes (QoS)

| Mode | Semantics | Flow Control | Best For |
| :--- | :--- | :--- | :--- |
| **`RELIABLE`** | Guaranteed & In-Order | Sliding window, selective ACKs, dynamic RTT, automatic retries | Firmware updates (OTA), mission-critical commands, configurations |
| **`UNRELIABLE`** | Fire-and-Forget | Immediate delivery, sequence loss tracking (`relinow_stats_t`) | High-frequency telemetry (IMU, temperature), periodic pings |
| **`PRIORITY`** | Latest-Wins | Single-slot mailbox (stale unacknowledged frames are discarded) | Real-time robotics, drone flight control, joystick steering |

---

## Physical Hardware Benchmarks (C vs Rust)

The protocol has been benchmarked on physical **ESP32-WROOM-32** hardware across two boards (COM4 and COM6) over 2.4 GHz ESP-NOW.

Each board streamed **1,000 sequential RELIABLE packets** (241 bytes payload each, totaling 241,000 bytes) with zero data loss.

![Relinow Benchmark: C vs Rust](benchmark_comparison.png)

### Performance Measurements

| Metric | C Native (ESP-IDF v5.2) | Rust (`esp-idf-sys` + FFI) | Notes |
| :--- | :---: | :---: | :---: |
| **Delivery Success Rate** | **1000 / 1000 (100.0%)** | **1000 / 1000 (100.0%)** | **Zero packet loss on both** |
| **Total Payload Transferred** | **241,000 bytes** (~235.35 KB) | **241,000 bytes** (~235.35 KB) | Max frame payload (241 B) |
| **Transfer Duration** | **10.03 seconds** | **15.98 seconds** | +5.95s in Rust (`dev` profile) |
| **Effective Throughput** | **23.47 KB/s** | **14.73 KB/s** | ~63% of native C speed |
| **Application Bitrate** | **0.19 Mbps** | **0.12 Mbps** | Usable application-layer throughput |
| **Average RTT (Latency)** | **14.0 ms** | **18.0 ms** | Only +4.0 ms overhead |

*Note: For an in-depth analysis of testbed RF conditions (laboratory line-of-sight vs. real-world multipath interference) and stress-test roadmaps, see the [Benchmark Documentation](docs/src/benchmarks.md).*

---

## Key Features

- **Micro-Header Framing**: 9-byte wire header, maximizing application payload (241 bytes per 250-byte ESP-NOW frame).
- **Per-Peer Adaptive RTO**: Implements Jacobson's algorithm with Exponential Weighted Moving Average (EWMA) and backoff.
- **Channel Multiplexing**: 16 virtual channels per peer with an integrated multi-pass scheduler.
- **Zero-Allocation**: All queues and state tables are bounded at compile time.
- **Packet Fragmentation**: Transparent segmentation and reassembly for payloads larger than 241 bytes.
- **Heartbeat Subsystem**: Channel `0xFF` keepalive ping/pong with missed-pong peer timeout detection.
- **Multi-Language**: Native C99 core + `#![no_std]` Rust crate + ESP-IDF FFI wrappers.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    User Application                         │
├──────────────────────────────┬──────────────────────────────┤
│         C99 API              │          Rust API            │
├──────────────────────────────┴──────────────────────────────┤
│               ReliNow Core Protocol Engine                  │
│   (Sliding Window · Adaptive RTO · Channel Scheduler · Reassembly)  │
├─────────────────────────────────────────────────────────────┤
│                 ESP-NOW Transport Adapter                   │
├─────────────────────────────────────────────────────────────┤
│               Espressif ESP-NOW (802.11 MAC)                │
└─────────────────────────────────────────────────────────────┘
```

---

## Project Structure

```
relinow/
├── core/                        ← Portable C99 core protocol engine
│   ├── include/                 ← Header definitions
│   │   ├── relinow_packet.h     ← Binary wire format & serialization
│   │   ├── relinow_state.h      ← State machines, channel tables
│   │   ├── relinow_reliable.h   ← Sliding window, ACK, adaptive RTO
│   │   └── relinow_espnow.h     ← ESP-IDF transport adapter
│   └── src/                     ← Implementation files
├── rust/                        ← Rust ecosystem
│   └── relinow/                 ← Pure #![no_std] Rust implementation
│       ├── src/                 ← Safe, zero-allocation Rust core
│       └── tests/               ← Property-based (proptest) & fuzz tests
├── examples/                    ← Hardware examples
│   ├── benchmark_c/             ← High-throughput C benchmark
│   ├── benchmark_rust/          ← High-throughput Rust benchmark
│   ├── esp32_a/                 ← Point-to-point node A
│   ├── esp32_b/                 ← Point-to-point node B
│   └── wokwi_demo/              ← Cloud simulation example
├── tests/                       ← Comprehensive test suite
│   ├── fixtures/                ← Automated binary test vectors
│   └── src/                     ← CTest verification suites
├── docs/                        ← mdBook documentation site
│   ├── book.toml                ← mdBook configuration
│   └── src/                     ← Architecture, API, and benchmark guides
├── plot_benchmark.py            ← Comparative benchmark graph generator
└── run_rust.ps1                 ← Automated Rust build & flash tool
```

---

## Getting Started

### Complete Documentation

To build and view the full mdBook documentation locally:
```bash
mdbook serve docs --open
```

### Running CTest Suite (Host C99 Engine)

Prerequisites: Python 3.10+, CMake 3.16+, and a C99 compiler (GCC, Clang, or MSVC).

```bash
# Generate test fixtures and run test matrix
python -m pip install -r tools/requirements.txt
python tools/build_fixtures.py --matrix-yaml TEST_MATRIX.yaml --vector-yaml TEST_VECTORS.yaml --out-dir tests/fixtures

cmake -S tests -B build/tests
cmake --build build/tests
ctest --test-dir build/tests --output-on-failure
```

### Running Rust Property-Based Tests (`no_std` Core)

```bash
cd rust/relinow
cargo test
```

### Flashing Physical ESP32 Benchmarks

1. **C Benchmark**:
   ```bash
   cd examples/benchmark_c
   idf.py -p COM4 flash
   idf.py -p COM6 flash
   ```

2. **Rust Benchmark**:
   ```powershell
   .\run_rust.ps1
   ```

---

## Status & Roadmap

- [x] Protocol binary specification ([PROTOCOL.md](PROTOCOL.md))
- [x] C99 zero-allocation core state engine
- [x] RELIABLE mode (Sliding window, selective ACK, ordering)
- [x] Jacobson's dynamic RTT calculation & exponential backoff
- [x] UNRELIABLE mode (fire-and-forget with loss metrics)
- [x] PRIORITY mode (newest-wins single-slot overwrite)
- [x] Channel multiplexing & multi-pass scheduler
- [x] Transparent packet fragmentation & reassembly
- [x] Heartbeat liveness monitoring (channel `0xFF`)
- [x] `#![no_std]` Rust implementation with property-based tests
- [x] Hardware verification on physical ESP32-WROOM dual-core chips
- [x] Comparative C vs Rust hardware benchmark and visual reports
- [x] Complete technical documentation ([mdBook](docs/))

---

## License

Distributed under the MIT License. See [LICENSE](LICENSE) for more information.

