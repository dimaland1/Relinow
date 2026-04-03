#include "fixture_loader.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifndef MATRIX_FIXTURE_PATH
#define MATRIX_FIXTURE_PATH "tests/fixtures/matrix_fixture.bin"
#endif

static int contains_invariant(const relinow_matrix_fixture_t* fixture, const char* invariant) {
    uint16_t i;
    for (i = 0; i < fixture->case_count; ++i) {
        if (strcmp(fixture->cases[i].invariant, invariant) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(void) {
    relinow_matrix_fixture_t fixture;
    int rc = relinow_load_matrix_fixture(MATRIX_FIXTURE_PATH, &fixture);

    if (rc != 0) {
        printf("matrix loader failed with rc=%d at path=%s\n", rc, MATRIX_FIXTURE_PATH);
        return 1;
    }

    assert(fixture.case_count > 0);
    assert(fixture.case_count >= 18);
    assert(strcmp(fixture.cases[0].id, "T001") == 0);
    assert(fixture.cases[0].expected_status_count > 0);

    assert(contains_invariant(&fixture, "INVARIANT_001") == 1);
    assert(contains_invariant(&fixture, "INVARIANT_002") == 1);
    assert(contains_invariant(&fixture, "INVARIANT_003") == 1);
    assert(contains_invariant(&fixture, "INVARIANT_004") == 1);
    assert(contains_invariant(&fixture, "INVARIANT_005") == 1);
    assert(contains_invariant(&fixture, "INVARIANT_006") == 1);
    assert(contains_invariant(&fixture, "INVARIANT_007") == 1);
    assert(contains_invariant(&fixture, "INVARIANT_008") == 1);
    assert(contains_invariant(&fixture, "INVARIANT_009") == 1);
    assert(contains_invariant(&fixture, "INVARIANT_010") == 1);
    assert(contains_invariant(&fixture, "INVARIANT_011") == 1);
    assert(contains_invariant(&fixture, "INVARIANT_012") == 1);
    assert(contains_invariant(&fixture, "INVARIANT_013") == 1);
    assert(contains_invariant(&fixture, "INVARIANT_014") == 1);
    assert(contains_invariant(&fixture, "INVARIANT_015") == 1);
    assert(contains_invariant(&fixture, "INVARIANT_016") == 1);
    assert(contains_invariant(&fixture, "INVARIANT_017") == 1);
    assert(contains_invariant(&fixture, "INVARIANT_018") == 1);

    printf("matrix loader ok: %u cases\n", fixture.case_count);
    return 0;
}
