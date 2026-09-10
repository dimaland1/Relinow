# PRIORITY Mode

**PRIORITY Mode** is designed for real-time control applications where only the latest state matters, and stale packets must never clog the communication queue.

---

## 1. The "Latest-Wins" Philosophy

In systems like drone flight controllers, robotic arms, or remote RC transmitters:
- If packet #5 (throttle = 80%) is delayed in the radio queue, and the pilot pushes throttle to 20% (packet #6), transmitting packet #5 creates dangerous control lag.
- **PRIORITY mode enforces single-slot mailbox semantics**: queueing a newer packet instantly displaces and drops any unsent or pending older packet on that channel.

---

## 2. Packet Flow

```
[New Command: Throttle 90%] ──> [PRIORITY Channel Slot] ──> Overwrites [Throttle 50%]
                                         │
                                         ▼
                               [Immediate Emission]
```

- **Transmitter**: Discards previous un-transmitted command buffers if a new sample arrives.
- **Receiver**: Discards out-of-order delayed frames if a newer sequence ID has already been consumed.
