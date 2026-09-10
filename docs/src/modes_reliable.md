# RELIABLE Mode

**RELIABLE Mode** provides guaranteed, in-order packet delivery over connectionless ESP-NOW links. It replicates the core reliability guarantees of TCP while eliminating TCP's heavy connection-handshake and packet-overhead penalties.

---

## 1. Algorithmic Principles

### Sliding Window Flow Control
- Each transmitting node maintains a fixed-size ring buffer of unacknowledged frames (`RELINOW_RELIABLE_MAX_PENDING`).
- Frames are tagged with a monotonically increasing 16-bit sequence ID (`seq_id`).
- When the receiver receives a packet, it emits an acknowledgment frame (`ACK`) containing the received `seq_id`.
- The transmitter slides its window forward upon ACK receipt, freeing buffer slots for new transmissions.

### Adaptive RTT Estimation (Jacobson's Algorithm)
Fixed timeouts fail in wireless networks due to interference and retransmissions. ReliNow dynamically tracks Round-Trip Time (RTT) per peer using an Exponentially Weighted Moving Average (EWMA):

$$\text{SRTT} \leftarrow (1 - \alpha) \cdot \text{SRTT} + \alpha \cdot \text{SampleRTT}$$
$$\text{RTTVAR} \leftarrow (1 - \beta) \cdot \text{RTTVAR} + \beta \cdot |\text{SRTT} - \text{SampleRTT}|$$
$$\text{RTO} \leftarrow \text{SRTT} + 4 \cdot \text{RTTVAR}$$

*(Standard parameters: $\alpha = 0.125$, $\beta = 0.25$)*

### Exponential Backoff
If a frame expires without an ACK, the Retransmission Timeout (RTO) doubles:
$$\text{RTO}_{\text{next}} = \min(\text{RTO} \times 2, \text{RTO}_{\text{max}})$$
This prevents network collapse under dense RF congestion.

---

## 2. Fragmentation & Reassembly

ESP-NOW restricts individual physical frames to 250 bytes (yielding 241 bytes of application payload after the 9-byte ReliNow header).

To transmit larger payloads (such as JSON blobs, cryptographic signatures, or OTA firmware blocks):
1. **Transmitter**: Slices payloads exceeding 241 bytes into consecutive sequence-numbered frames. Intermediate fragments carry the `RELINOW_FLAG_FRAGMENT` flag, and the final chunk carries `RELINOW_FLAG_LAST_FRAGMENT`.
2. **Receiver**: Accumulates incoming chunks in a dedicated reassembly buffer.
3. **Delivery**: The complete reconstructed message is delivered to `on_message` as a single contiguous buffer once the final fragment arrives.
4. **Safety Timeout**: If an intermediate fragment is permanently lost, `fragment_timeout_ms` safely purges stale buffers to prevent memory leakage.
