#include "fixture_loader.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifndef VECTOR_FIXTURE_PATH
#define VECTOR_FIXTURE_PATH "tests/fixtures/vector_fixture.bin"
#endif

int main(void) {
    relinow_vector_fixture_t fixture;
    int rc = relinow_load_vector_fixture(VECTOR_FIXTURE_PATH, &fixture);

    if (rc != 0) {
        printf("vector loader failed with rc=%d at path=%s\n", rc, VECTOR_FIXTURE_PATH);
        return 1;
    }

    assert(fixture.case_count > 0);
    assert(strcmp(fixture.cases[0].id, "V001") == 0);
    assert(fixture.cases[0].frame_len >= 9);

    printf("vector loader ok: %u cases\n", fixture.case_count);
    return 0;
}
