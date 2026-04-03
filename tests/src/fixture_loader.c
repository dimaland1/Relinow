#include "fixture_loader.h"

#include <stdio.h>
#include <string.h>

#define RELINOW_MAGIC_LEN 8

static const unsigned char k_matrix_magic[RELINOW_MAGIC_LEN] = {'R', 'L', 'N', 'M', 'T', 'X', '0', '1'};
static const unsigned char k_vector_magic[RELINOW_MAGIC_LEN] = {'R', 'L', 'N', 'V', 'E', 'C', '0', '1'};

static int read_exact(FILE* file, void* out, size_t size) {
    return fread(out, 1, size, file) == size ? 0 : -1;
}

static int read_u16_le(FILE* file, unsigned short* out_value) {
    unsigned char bytes[2];
    if (read_exact(file, bytes, sizeof(bytes)) != 0) {
        return -1;
    }
    *out_value = (unsigned short)(bytes[0] | ((unsigned short)bytes[1] << 8));
    return 0;
}

static int read_text_field(FILE* file, char* out, size_t size) {
    if (read_exact(file, out, size) != 0) {
        return -1;
    }
    out[size - 1] = '\0';
    return 0;
}

static int check_magic(FILE* file, const unsigned char* expected) {
    unsigned char magic[RELINOW_MAGIC_LEN];
    if (read_exact(file, magic, sizeof(magic)) != 0) {
        return -1;
    }
    return memcmp(magic, expected, RELINOW_MAGIC_LEN) == 0 ? 0 : -1;
}

int relinow_load_matrix_fixture(const char* path, relinow_matrix_fixture_t* out_fixture) {
    FILE* file;
    unsigned short case_count;
    unsigned short i;

    if (path == NULL || out_fixture == NULL) {
        return -1;
    }

    memset(out_fixture, 0, sizeof(*out_fixture));
    file = fopen(path, "rb");
    if (file == NULL) {
        return -2;
    }

    if (check_magic(file, k_matrix_magic) != 0) {
        fclose(file);
        return -3;
    }
    if (read_u16_le(file, &case_count) != 0 || case_count > RELINOW_MAX_CASES) {
        fclose(file);
        return -4;
    }

    out_fixture->case_count = case_count;
    for (i = 0; i < case_count; ++i) {
        relinow_matrix_case_t* c = &out_fixture->cases[i];
        unsigned short slot;

        if (read_text_field(file, c->id, RELINOW_MAX_ID) != 0 ||
            read_text_field(file, c->invariant, RELINOW_MAX_INVARIANT) != 0 ||
            read_text_field(file, c->description, RELINOW_MAX_TEXT) != 0 ||
            read_text_field(file, c->setup, RELINOW_MAX_TEXT) != 0 ||
            read_exact(file, &c->section, 1) != 0 ||
            read_exact(file, &c->expected_status_count, 1) != 0 ||
            read_exact(file, c->expected_status, RELINOW_MAX_STATUSES) != 0 ||
            read_exact(file, &c->input_count, 1) != 0) {
            fclose(file);
            return -5;
        }

        if (c->expected_status_count > RELINOW_MAX_STATUSES || c->input_count > RELINOW_MAX_INPUTS) {
            fclose(file);
            return -6;
        }

        for (slot = 0; slot < RELINOW_MAX_INPUTS; ++slot) {
            if (read_text_field(file, c->inputs[slot], RELINOW_MAX_TEXT) != 0) {
                fclose(file);
                return -7;
            }
        }

        if (read_exact(file, &c->effect_count, 1) != 0 || c->effect_count > RELINOW_MAX_EFFECTS) {
            fclose(file);
            return -8;
        }

        for (slot = 0; slot < RELINOW_MAX_EFFECTS; ++slot) {
            if (read_text_field(file, c->effects[slot], RELINOW_MAX_TEXT) != 0) {
                fclose(file);
                return -9;
            }
        }
    }

    fclose(file);
    return 0;
}

int relinow_load_vector_fixture(const char* path, relinow_vector_fixture_t* out_fixture) {
    FILE* file;
    unsigned short case_count;
    unsigned short i;

    if (path == NULL || out_fixture == NULL) {
        return -1;
    }

    memset(out_fixture, 0, sizeof(*out_fixture));
    file = fopen(path, "rb");
    if (file == NULL) {
        return -2;
    }

    if (check_magic(file, k_vector_magic) != 0) {
        fclose(file);
        return -3;
    }
    if (read_u16_le(file, &case_count) != 0 || case_count > RELINOW_MAX_CASES) {
        fclose(file);
        return -4;
    }

    out_fixture->case_count = case_count;
    for (i = 0; i < case_count; ++i) {
        relinow_vector_case_t* c = &out_fixture->cases[i];

        if (read_text_field(file, c->id, RELINOW_MAX_ID) != 0 ||
            read_exact(file, &c->expect_valid, 1) != 0 ||
            read_exact(file, &c->mode, 1) != 0 ||
            read_exact(file, &c->type, 1) != 0 ||
            read_exact(file, &c->flags, 1) != 0 ||
            read_u16_le(file, &c->seq_id) != 0 ||
            read_u16_le(file, &c->ack_id) != 0 ||
            read_exact(file, &c->channel_id, 1) != 0 ||
            read_u16_le(file, &c->payload_len) != 0 ||
            read_u16_le(file, &c->frame_len) != 0) {
            fclose(file);
            return -5;
        }

        if (c->frame_len > 512 || c->payload_len > 503 || c->payload_len + 9 < c->frame_len) {
            fclose(file);
            return -6;
        }

        if (read_exact(file, c->frame, 512) != 0) {
            fclose(file);
            return -7;
        }
    }

    fclose(file);
    return 0;
}
