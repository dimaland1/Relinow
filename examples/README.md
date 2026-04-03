# ReliNow ESP-NOW Hardware Examples (A/B)

This folder contains two ESP-IDF projects ready to flash on two ESP32 boards:

- `examples/esp32_a`: sends reliable messages every second.
- `examples/esp32_b`: receives + ACKs and sends a heartbeat every 3 seconds.

Both examples wire the ReliNow RELIABLE path to real ESP-NOW callbacks (`send_cb`, `recv_cb`) and run retransmission polling in the main loop.

## Prerequisites

- ESP-IDF installed and exported (`idf.py` available)
- Two ESP32 boards (A and B)
- USB serial access for both boards

## 1) Configure MAC addresses

1. Flash once and read each board local STA MAC from logs (`Local MAC: ...`).
2. Update peer constants:
   - `examples/esp32_a/main/main.c` -> `PEER_MAC` = board B MAC
   - `examples/esp32_b/main/main.c` -> `PEER_MAC` = board A MAC

Both examples use Wi-Fi channel `1` by default.

## 2) Flash board A

```bash
cd examples/esp32_a
idf.py set-target esp32
idf.py -p <PORT_A> flash monitor
```

## 3) Flash board B

```bash
cd examples/esp32_b
idf.py set-target esp32
idf.py -p <PORT_B> flash monitor
```

## 4) Expected runtime behavior

- Board A prints `TX new seq=...` and regular `A->B reliable msg #...`
- Board B prints received messages with sequence numbers in order
- If radio drops packets, you should see `TX retransmit seq=...`
- If retries exceed threshold, `TX failed seq=...` is reported

## 5) Quick radio stress checks

- Move boards farther apart or add interference.
- Confirm retransmissions appear and delivery still recovers.
- Confirm duplicates are not re-delivered and order remains monotonic.

## Notes

- This is a RELIABLE MVP path: DATA + ACK + retransmission + ordering baseline.
- For production use, tune retry and RTT config in `relinow_espnow_default_config()` / per-node config.
