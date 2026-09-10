# Transmission Modes (QoS)

ReliNow provides three distinct transmission modes, allowing embedded developers to map disparate traffic types to the optimal balance of reliability, latency, and throughput:

```
┌─────────────────────────────────────────────────────────────────┐
│                     ReliNow QoS Spectrum                        │
├───────────────────┬──────────────────────┬──────────────────────┤
│     RELIABLE      │      UNRELIABLE      │       PRIORITY       │
│  Guaranteed & In- │   Fire-and-Forget    │     Latest-Wins      │
│     Sequence      │   High-Frequency     │   Real-Time Control  │
└───────────────────┴──────────────────────┴──────────────────────┘
```

---

## Mode Comparison Matrix

| Feature | RELIABLE | UNRELIABLE | PRIORITY |
| :--- | :---: | :---: | :---: |
| **Acknowledgments (ACK)** | Yes (Selective ACK) | No | No (Explicit ACK bypass) |
| **Automatic Retries** | Yes (Exponential RTO) | No | No |
| **In-Order Delivery** | Yes (Sliding Window) | No (Immediate Delivery) | Latest sample only |
| **Fragmentation Support**| Yes (> 241 bytes) | No (Single Frame) | No (Single Frame) |
| **Loss Tracking** | 0% tolerated (Retried)| Yes (`relinow_stats_t`) | Overwritten if stale |
| **Typical Use Cases** | Firmware updates (OTA), critical alarms, configuration commands | Sensor telemetry (temperature, IMU streaming), periodic pings | RC flight control, robotics steering, gimbal setpoints |
