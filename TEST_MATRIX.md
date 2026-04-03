# ReliNow Protocol Invariant Test Matrix

This file contains a ready-to-run test matrix for protocol invariants INVARIANT_001 to INVARIANT_018.

## Coverage Matrix

| Test ID | Invariant | Type | Preconditions | Stimulus | Expected Result |
|---|---|---|---|---|---|
| T001 | INVARIANT_001: Header size is exactly 9 bytes | Negative | Raw parser available | Feed 8-byte packet, then 9-byte header | 8 bytes: discard invalid packet. 9-byte header: parse allowed. |
| T002 | INVARIANT_002: Version must be 0x01 | Positive + Negative | Parser initialized | Version=0x01, then Version=0x02 | 0x01 accepted. 0x02 silently discarded. |
| T003 | INVARIANT_003: Mode in {0x01,0x02,0x03} | Positive + Negative | Parser initialized | Mode=0x01/0x02/0x03, then 0x00/0x04 | Valid modes accepted. Others discarded as invalid packet. |
| T004 | INVARIANT_004: Type in 0x01..0x05 | Positive + Negative | Parser initialized | Type=0x01..0x05, then 0x00/0x06 | Valid types accepted. Out-of-range types discarded. |
| T005 | INVARIANT_005: Reserved flag bit3 must be 0 | Positive + Negative | Parser initialized | Flags with bit3=0, then bit3=1 | bit3=0 accepted. bit3=1 discarded. |
| T006 | INVARIANT_006: Payload length <= max_payload | Negative boundary | max_payload configured (ESP-NOW v1 or v2) | payload_len=max_payload, then max_payload+1 | max accepted. max+1 rejected with payload-too-large behavior. |
| T007 | INVARIANT_007: payload_len matches remaining bytes | Negative | Raw parser available | Header says payload_len=20, buffer carries 15 payload bytes | Packet discarded as malformed length mismatch. |
| T008 | INVARIANT_008: Channel 0xFF reserved for heartbeat only | Positive + Negative | Parser initialized | DATA/ACK on channel 0xFF, then PING/PONG on 0xFF | DATA/ACK on 0xFF rejected. PING/PONG on 0xFF accepted. |
| T009 | INVARIANT_009: Sequence wraps 65535 -> 0 | Positive | Sequence counter starts at 65534 | Send three packets | Sequence IDs are 65534, 65535, 0 in order. |
| T010 | INVARIANT_010: Newer rule via int16 delta | Positive + Edge | is_newer(a,b) helper available | Evaluate pairs: (0,65535), (1,0), (65535,0), (100,100) | True, True, False, False respectively. |
| T011 | INVARIANT_011: ACK payload_len must be 0 | Positive + Negative | Parser initialized | ACK with payload_len=0, then ACK with payload_len=1 | payload_len=0 accepted. payload_len>0 rejected. |
| T012 | INVARIANT_012: PING/PONG must use channel 0xFF | Positive + Negative | Parser initialized | PING/PONG on 0xFF, then on channel 0x01 | 0xFF accepted. Non-0xFF rejected. |
| T013 | INVARIANT_013: UNRELIABLE packets must have ack_id=0 | Positive + Negative | Channel mode is UNRELIABLE | DATA with ack_id=0, then ack_id=42 | ack_id=0 accepted. ack_id!=0 rejected (or normalized to 0 if policy says so). |
| T014 | INVARIANT_014: FRAGMENT valid only in RELIABLE mode | Positive + Negative | Modes RELIABLE/UNRELIABLE/PRIORITY available | Set FRAGMENT=1 in each mode | RELIABLE accepted. UNRELIABLE and PRIORITY rejected. |
| T015 | INVARIANT_015: LAST_FRAGMENT implies FRAGMENT | Negative | Parser initialized | LAST_FRAGMENT=1 with FRAGMENT=0 | Packet rejected as inconsistent flags. |
| T016 | INVARIANT_016: PRIORITY max one in-flight packet per (peer,channel) | State-machine | PRIORITY channel open, packet A in-flight | Send packet B before ACK of A | A is superseded/removed. Only B remains in-flight. |
| T017 | INVARIANT_017: Serialize->Deserialize round-trip | Positive | Valid randomized header fixture | Serialize header, then deserialize | All parsed field values equal original values. |
| T018 | INVARIANT_018: Big-endian multi-byte fields | Positive + Byte-level | Known values for seq_id, ack_id, payload_len | Serialize header and inspect bytes [2..8] | seq_id, ack_id, payload_len encoded in big-endian order. |

## Optional Add-On Invariants (Recommended)

These are not in the original list but are strongly recommended for production behavior tests.

| Test ID | Invariant | Type | Expected Result |
|---|---|---|---|
| T019 | Fragmentation forbidden in UNRELIABLE and PRIORITY | Negative | Sender rejects oversized payload/fragment flags outside RELIABLE. |
| T020 | RELIABLE delivers to app in-order only | Behavioral | Out-of-order packets buffered until gap is resolved or timeout policy triggers. |
| T021 | RELIABLE duplicate handling | Behavioral | Duplicate packet discarded; ACK may be re-sent. |
| T022 | One fragmented message in-flight per channel | State-machine | Interleaved fragmented messages on same channel are rejected/deferred. |
| T023 | Application cannot open channel 0xFF | API contract | Open-channel call on 0xFF fails with invalid parameter. |

## Suggested Test Case Schema

Use this schema in your test framework to keep all cases consistent:

- id
- invariant
- setup
- input
- expected_status (ACCEPT, DISCARD, ERR_xxx)
- expected_side_effects (ack sent, queue mutation, stats update, callback)
