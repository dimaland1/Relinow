#ifndef RELINOW_PACKET_H
#define RELINOW_PACKET_H

#include <stddef.h>
#include <stdint.h>

#define RELINOW_HEADER_SIZE 9u
#define RELINOW_PROTOCOL_VERSION 0x01u

#define RELINOW_MODE_RELIABLE 0x01u
#define RELINOW_MODE_UNRELIABLE 0x02u
#define RELINOW_MODE_PRIORITY 0x03u

#define RELINOW_TYPE_DATA 0x01u
#define RELINOW_TYPE_ACK 0x02u
#define RELINOW_TYPE_NACK 0x03u
#define RELINOW_TYPE_PING 0x04u
#define RELINOW_TYPE_PONG 0x05u

#define RELINOW_FLAG_FRAGMENT 0x01u
#define RELINOW_FLAG_LAST_FRAGMENT 0x02u
#define RELINOW_FLAG_ENCRYPTED 0x04u
#define RELINOW_FLAG_RESERVED 0x08u

#define RELINOW_HEARTBEAT_CHANNEL 0xFFu

typedef enum {
    RELINOW_ERR_OK = 0,
    RELINOW_ERR_INVALID_ARG = 1,
    RELINOW_ERR_INVALID_PACKET = 2,
    RELINOW_ERR_PAYLOAD_TOO_LARGE = 3
} relinow_err_t;

typedef struct {
    uint8_t version;
    uint8_t mode;
    uint8_t type;
    uint8_t flags;
    uint16_t seq_id;
    uint16_t ack_id;
    uint8_t channel_id;
    uint16_t payload_len;
} relinow_header_t;

relinow_err_t relinow_encode_header(
    const relinow_header_t* header,
    uint16_t max_payload,
    uint8_t out_bytes[RELINOW_HEADER_SIZE]
);

relinow_err_t relinow_decode_header(
    const uint8_t* frame,
    size_t frame_len,
    uint16_t max_payload,
    relinow_header_t* out_header
);

relinow_err_t relinow_validate_header(
    const relinow_header_t* header,
    uint16_t max_payload
);

int relinow_is_seq_newer(uint16_t seq_a, uint16_t seq_b);
uint16_t relinow_seq_next(uint16_t current);

#endif
