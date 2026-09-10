# UNRELIABLE Mode

**UNRELIABLE Mode** delivers raw, fire-and-forget datagram performance with zero acknowledgment overhead, while providing real-time channel statistics.

---

## 1. Principles & Design

In high-frequency IoT streaming (such as IMU sensors, ambient temperature readings, or audio pings), retransmitting an outdated measurement is counterproductive: the next measurement will arrive in milliseconds anyway.

UNRELIABLE Mode features:
- **Zero Retransmission Overhead**: Packets are transmitted once and never re-sent.
- **Immediate Delivery**: Packets bypass the reordering queue and are immediately passed to the application callback upon receipt.
- **Loss Accounting**: Each packet carries a sequential `seq_id`. When a sequence gap is detected by the receiver, the missing count is recorded in link statistics.

---

## 2. Link Quality Statistics (`relinow_stats_t`)

The receiver continuously calculates the health of the radio link using sequence numbers:

```c
typedef struct {
    uint32_t total_rx;   // Total successfully received UNRELIABLE frames
    uint32_t total_lost; // Total missing sequence frames detected
} relinow_stats_t;

// Query link health
relinow_stats_t stats;
relinow_state_get_stats(&node.state, peer_idx, channel_id, &stats);
float packet_loss_rate = (float)stats.total_lost / (stats.total_rx + stats.total_lost);
```
