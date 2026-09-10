# Introduction to ReliNow

**ReliNow** is a lightweight, zero-allocation, robust transport protocol built specifically for embedded systems. It was originally designed to sit on top of the **ESP-NOW** MAC layer (Espressif's connectionless Wi-Fi communication protocol), bringing TCP-like reliability and multiplexing without the massive overhead of a full IP stack.

## Why ReliNow?

ESP-NOW is fantastic for bare-metal IoT. It allows microcontrollers to exchange data with extremely low latency (often < 2ms) without needing a router. However, ESP-NOW has limitations:
- **No inherent reliability:** If a packet is lost due to interference, it's gone.
- **Limited payload size:** Maximum 250 bytes per frame.
- **No logical channels:** All data goes to a single callback, making it hard to separate telemetry from OTA updates or commands.

ReliNow solves all of this by implementing:
1. **Guaranteed Delivery (RELIABLE mode)** with sliding windows, automatic retries, and dynamic RTT (Round-Trip Time) estimation.
2. **Logical Multiplexing (Channels)** allowing up to 16 virtual channels per peer.
3. **Different QoS (Quality of Service) Modes**: Reliable, Unreliable, and Priority.
4. **Heartbeat & Liveness tracking** to detect dead nodes.

## Multi-Language Support

ReliNow is written in strict **C99** (pure, no dependencies) making it compatible with any microcontroller compiler (GCC, Clang, IAR, Keil). 
Additionally, a safe, `#![no_std]` **Rust Wrapper** is provided to seamlessly integrate the protocol into modern Rust embedded applications (`esp-hal` / `esp-wifi`).
