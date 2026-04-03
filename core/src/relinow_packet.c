#include "relinow_packet.h"

static int relinow_is_valid_mode(uint8_t mode) {
    return mode == RELINOW_MODE_RELIABLE || mode == RELINOW_MODE_UNRELIABLE || mode == RELINOW_MODE_PRIORITY;
}

static int relinow_is_valid_type(uint8_t type) {
    return type >= RELINOW_TYPE_DATA && type <= RELINOW_TYPE_PONG;
}

relinow_err_t relinow_validate_header(const relinow_header_t* header, uint16_t max_payload) {
    if (header == 0) {
        return RELINOW_ERR_INVALID_ARG;
    }
    if (header->version != RELINOW_PROTOCOL_VERSION) {
        return RELINOW_ERR_INVALID_PACKET;
    }
    if (!relinow_is_valid_mode(header->mode) || !relinow_is_valid_type(header->type)) {
        return RELINOW_ERR_INVALID_PACKET;
    }
    if ((header->flags & RELINOW_FLAG_RESERVED) != 0u) {
        return RELINOW_ERR_INVALID_PACKET;
    }
    if (header->payload_len > max_payload) {
        return RELINOW_ERR_PAYLOAD_TOO_LARGE;
    }

    if (header->channel_id == RELINOW_HEARTBEAT_CHANNEL) {
        if (header->type != RELINOW_TYPE_PING && header->type != RELINOW_TYPE_PONG) {
            return RELINOW_ERR_INVALID_PACKET;
        }
    } else if (header->type == RELINOW_TYPE_PING || header->type == RELINOW_TYPE_PONG) {
        return RELINOW_ERR_INVALID_PACKET;
    }

    if (header->type == RELINOW_TYPE_ACK && header->payload_len != 0u) {
        return RELINOW_ERR_INVALID_PACKET;
    }

    if (header->mode == RELINOW_MODE_UNRELIABLE && header->ack_id != 0u) {
        return RELINOW_ERR_INVALID_PACKET;
    }

    if ((header->flags & RELINOW_FLAG_FRAGMENT) != 0u && header->mode != RELINOW_MODE_RELIABLE) {
        return RELINOW_ERR_INVALID_PACKET;
    }

    if ((header->flags & RELINOW_FLAG_LAST_FRAGMENT) != 0u && (header->flags & RELINOW_FLAG_FRAGMENT) == 0u) {
        return RELINOW_ERR_INVALID_PACKET;
    }

    return RELINOW_ERR_OK;
}

relinow_err_t relinow_encode_header(
    const relinow_header_t* header,
    uint16_t max_payload,
    uint8_t out_bytes[RELINOW_HEADER_SIZE]
) {
    relinow_err_t validation;
    if (header == 0 || out_bytes == 0) {
        return RELINOW_ERR_INVALID_ARG;
    }

    validation = relinow_validate_header(header, max_payload);
    if (validation != RELINOW_ERR_OK) {
        return validation;
    }

    out_bytes[0] = (uint8_t)(((header->version & 0x0Fu) << 4) | (header->mode & 0x0Fu));
    out_bytes[1] = (uint8_t)(((header->type & 0x0Fu) << 4) | (header->flags & 0x0Fu));
    out_bytes[2] = (uint8_t)(header->seq_id >> 8);
    out_bytes[3] = (uint8_t)(header->seq_id & 0xFFu);
    out_bytes[4] = (uint8_t)(header->ack_id >> 8);
    out_bytes[5] = (uint8_t)(header->ack_id & 0xFFu);
    out_bytes[6] = header->channel_id;
    out_bytes[7] = (uint8_t)(header->payload_len >> 8);
    out_bytes[8] = (uint8_t)(header->payload_len & 0xFFu);

    return RELINOW_ERR_OK;
}

relinow_err_t relinow_decode_header(
    const uint8_t* frame,
    size_t frame_len,
    uint16_t max_payload,
    relinow_header_t* out_header
) {
    relinow_err_t validation;

    if (frame == 0 || out_header == 0) {
        return RELINOW_ERR_INVALID_ARG;
    }
    if (frame_len < RELINOW_HEADER_SIZE) {
        return RELINOW_ERR_INVALID_PACKET;
    }

    out_header->version = (uint8_t)((frame[0] >> 4) & 0x0Fu);
    out_header->mode = (uint8_t)(frame[0] & 0x0Fu);
    out_header->type = (uint8_t)((frame[1] >> 4) & 0x0Fu);
    out_header->flags = (uint8_t)(frame[1] & 0x0Fu);
    out_header->seq_id = (uint16_t)(((uint16_t)frame[2] << 8) | frame[3]);
    out_header->ack_id = (uint16_t)(((uint16_t)frame[4] << 8) | frame[5]);
    out_header->channel_id = frame[6];
    out_header->payload_len = (uint16_t)(((uint16_t)frame[7] << 8) | frame[8]);

    if ((size_t)out_header->payload_len != (frame_len - RELINOW_HEADER_SIZE)) {
        return RELINOW_ERR_INVALID_PACKET;
    }

    validation = relinow_validate_header(out_header, max_payload);
    return validation;
}

int relinow_is_seq_newer(uint16_t seq_a, uint16_t seq_b) {
    return (int16_t)(seq_a - seq_b) > 0;
}

uint16_t relinow_seq_next(uint16_t current) {
    return (uint16_t)(current + 1u);
}
