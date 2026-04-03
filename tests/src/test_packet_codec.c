#include "relinow_packet.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define MAX_PAYLOAD_V1 241u

static relinow_header_t make_valid_header(void) {
    relinow_header_t header;
    memset(&header, 0, sizeof(header));
    header.version = RELINOW_PROTOCOL_VERSION;
    header.mode = RELINOW_MODE_RELIABLE;
    header.type = RELINOW_TYPE_DATA;
    header.flags = 0u;
    header.seq_id = 42u;
    header.ack_id = 10u;
    header.channel_id = 1u;
    header.payload_len = 5u;
    return header;
}

static void test_round_trip(void) {
    relinow_header_t input = make_valid_header();
    relinow_header_t output;
    uint8_t frame[RELINOW_HEADER_SIZE + 5u] = {0};

    assert(relinow_encode_header(&input, MAX_PAYLOAD_V1, frame) == RELINOW_ERR_OK);
    memcpy(&frame[RELINOW_HEADER_SIZE], "HELLO", 5u);

    assert(relinow_decode_header(frame, sizeof(frame), MAX_PAYLOAD_V1, &output) == RELINOW_ERR_OK);
    assert(output.version == input.version);
    assert(output.mode == input.mode);
    assert(output.type == input.type);
    assert(output.flags == input.flags);
    assert(output.seq_id == input.seq_id);
    assert(output.ack_id == input.ack_id);
    assert(output.channel_id == input.channel_id);
    assert(output.payload_len == input.payload_len);
}

static void test_endianness(void) {
    relinow_header_t header = make_valid_header();
    uint8_t bytes[RELINOW_HEADER_SIZE];

    header.seq_id = 0x1234u;
    header.ack_id = 0xABCDu;
    header.payload_len = 0x00F0u;

    assert(relinow_encode_header(&header, MAX_PAYLOAD_V1, bytes) == RELINOW_ERR_OK);
    assert(bytes[2] == 0x12u);
    assert(bytes[3] == 0x34u);
    assert(bytes[4] == 0xABu);
    assert(bytes[5] == 0xCDu);
    assert(bytes[7] == 0x00u);
    assert(bytes[8] == 0xF0u);
}

static void test_invalid_values(void) {
    relinow_header_t header = make_valid_header();
    uint8_t bytes[RELINOW_HEADER_SIZE];

    header.version = 0x02u;
    assert(relinow_encode_header(&header, MAX_PAYLOAD_V1, bytes) == RELINOW_ERR_INVALID_PACKET);

    header = make_valid_header();
    header.mode = 0x04u;
    assert(relinow_encode_header(&header, MAX_PAYLOAD_V1, bytes) == RELINOW_ERR_INVALID_PACKET);

    header = make_valid_header();
    header.type = 0x00u;
    assert(relinow_encode_header(&header, MAX_PAYLOAD_V1, bytes) == RELINOW_ERR_INVALID_PACKET);

    header = make_valid_header();
    header.flags = RELINOW_FLAG_RESERVED;
    assert(relinow_encode_header(&header, MAX_PAYLOAD_V1, bytes) == RELINOW_ERR_INVALID_PACKET);
}

static void test_bounds_and_length(void) {
    relinow_header_t header = make_valid_header();
    uint8_t frame[RELINOW_HEADER_SIZE + 6u] = {0};

    header.payload_len = MAX_PAYLOAD_V1;
    assert(relinow_validate_header(&header, MAX_PAYLOAD_V1) == RELINOW_ERR_OK);

    header.payload_len = (uint16_t)(MAX_PAYLOAD_V1 + 1u);
    assert(relinow_validate_header(&header, MAX_PAYLOAD_V1) == RELINOW_ERR_PAYLOAD_TOO_LARGE);

    header = make_valid_header();
    assert(relinow_encode_header(&header, MAX_PAYLOAD_V1, frame) == RELINOW_ERR_OK);
    memcpy(&frame[RELINOW_HEADER_SIZE], "HELLO!", 6u);
    assert(relinow_decode_header(frame, sizeof(frame), MAX_PAYLOAD_V1, &header) == RELINOW_ERR_INVALID_PACKET);
}

static void test_protocol_rules(void) {
    relinow_header_t header = make_valid_header();

    header.type = RELINOW_TYPE_ACK;
    header.payload_len = 1u;
    assert(relinow_validate_header(&header, MAX_PAYLOAD_V1) == RELINOW_ERR_INVALID_PACKET);

    header = make_valid_header();
    header.channel_id = RELINOW_HEARTBEAT_CHANNEL;
    assert(relinow_validate_header(&header, MAX_PAYLOAD_V1) == RELINOW_ERR_INVALID_PACKET);

    header = make_valid_header();
    header.type = RELINOW_TYPE_PING;
    header.channel_id = 1u;
    assert(relinow_validate_header(&header, MAX_PAYLOAD_V1) == RELINOW_ERR_INVALID_PACKET);

    header = make_valid_header();
    header.mode = RELINOW_MODE_UNRELIABLE;
    header.ack_id = 99u;
    assert(relinow_validate_header(&header, MAX_PAYLOAD_V1) == RELINOW_ERR_INVALID_PACKET);

    header = make_valid_header();
    header.mode = RELINOW_MODE_UNRELIABLE;
    header.flags = RELINOW_FLAG_FRAGMENT;
    assert(relinow_validate_header(&header, MAX_PAYLOAD_V1) == RELINOW_ERR_INVALID_PACKET);

    header = make_valid_header();
    header.flags = RELINOW_FLAG_LAST_FRAGMENT;
    assert(relinow_validate_header(&header, MAX_PAYLOAD_V1) == RELINOW_ERR_INVALID_PACKET);
}

static void test_sequence_helpers(void) {
    assert(relinow_is_seq_newer(0u, 65535u) == 1);
    assert(relinow_is_seq_newer(1u, 0u) == 1);
    assert(relinow_is_seq_newer(65535u, 0u) == 0);
    assert(relinow_is_seq_newer(100u, 100u) == 0);
    assert(relinow_seq_next(65535u) == 0u);
}

int main(void) {
    test_round_trip();
    test_endianness();
    test_invalid_values();
    test_bounds_and_length();
    test_protocol_rules();
    test_sequence_helpers();

    printf("packet codec tests ok\n");
    return 0;
}
