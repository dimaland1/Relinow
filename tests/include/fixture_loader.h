#ifndef RELINOW_FIXTURE_LOADER_H
#define RELINOW_FIXTURE_LOADER_H

#include <stdint.h>

#define RELINOW_MAX_CASES 64
#define RELINOW_MAX_STATUSES 8
#define RELINOW_MAX_INPUTS 8
#define RELINOW_MAX_EFFECTS 8
#define RELINOW_MAX_TEXT 192
#define RELINOW_MAX_ID 24
#define RELINOW_MAX_INVARIANT 40

typedef enum {
    RELINOW_STATUS_UNKNOWN = 0,
    RELINOW_STATUS_ACCEPT = 1,
    RELINOW_STATUS_DISCARD = 2,
    RELINOW_STATUS_ERR_INVALID_PACKET = 3,
    RELINOW_STATUS_ERR_PAYLOAD_TOO_LARGE = 4,
    RELINOW_STATUS_ERR_INVALID_CHANNEL = 5,
    RELINOW_STATUS_ERR_INVALID_FLAGS = 6,
    RELINOW_STATUS_ERR_INVALID_ACK = 7
} relinow_expected_status_t;

typedef struct {
    char id[RELINOW_MAX_ID];
    char invariant[RELINOW_MAX_INVARIANT];
    char description[RELINOW_MAX_TEXT];
    char setup[RELINOW_MAX_TEXT];
    uint8_t section;
    uint8_t expected_status_count;
    uint8_t expected_status[RELINOW_MAX_STATUSES];
    uint8_t input_count;
    char inputs[RELINOW_MAX_INPUTS][RELINOW_MAX_TEXT];
    uint8_t effect_count;
    char effects[RELINOW_MAX_EFFECTS][RELINOW_MAX_TEXT];
} relinow_matrix_case_t;

typedef struct {
    uint16_t case_count;
    relinow_matrix_case_t cases[RELINOW_MAX_CASES];
} relinow_matrix_fixture_t;

typedef struct {
    char id[RELINOW_MAX_ID];
    uint8_t expect_valid;
    uint8_t mode;
    uint8_t type;
    uint8_t flags;
    uint16_t seq_id;
    uint16_t ack_id;
    uint8_t channel_id;
    uint16_t payload_len;
    uint16_t frame_len;
    uint8_t frame[512];
} relinow_vector_case_t;

typedef struct {
    uint16_t case_count;
    relinow_vector_case_t cases[RELINOW_MAX_CASES];
} relinow_vector_fixture_t;

int relinow_load_matrix_fixture(const char* path, relinow_matrix_fixture_t* out_fixture);
int relinow_load_vector_fixture(const char* path, relinow_vector_fixture_t* out_fixture);

#endif
