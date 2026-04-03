# ReliNow Fixture Tests

This folder provides:

- A C fixture loader for binary test data.
- Unit tests for packet codec and state core.
- A CMake build configuration.

## Files

- `include/fixture_loader.h`: public fixture structures and loader API.
- `src/fixture_loader.c`: binary fixture parser.
- `src/test_matrix_loader.c`: verifies matrix load and INVARIANT_001..018 coverage.
- `src/test_vector_loader.c`: vector fixture loader smoke test.
- `src/test_packet_codec.c`: header encode/decode and strict validation tests.
- `src/test_state_core.c`: static peer/channel state table tests.
- `src/test_reliable_mvp.c`: RELIABLE MVP (DATA/ACK, retransmission, ordering, RTT/backoff) tests.
- `src/test_reliable_state.c`: RELIABLE integration tests through peer/channel state APIs.
- `fixtures/*.bin`: generated binary fixtures from YAML.

## Generate Fixtures

From repository root:

```bash
python -m pip install -r tools/requirements.txt
python tools/build_fixtures.py --matrix-yaml TEST_MATRIX.yaml --vector-yaml TEST_VECTORS.yaml --out-dir tests/fixtures
```

## Build and Run Tests

From repository root:

```bash
cmake -S tests -B build/tests
cmake --build build/tests
ctest --test-dir build/tests -C Debug --output-on-failure
```
