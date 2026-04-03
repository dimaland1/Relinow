cmake_minimum_required(VERSION 3.16)
project(relinow_fixture_tests C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(relinow_core
    ../core/src/relinow_packet.c
    ../core/src/relinow_state.c
    ../core/src/relinow_reliable.c
)

target_include_directories(relinow_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
)

add_library(relinow_fixture_loader
    src/fixture_loader.c
)

target_include_directories(relinow_fixture_loader PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(test_matrix_loader
    src/test_matrix_loader.c
)
target_link_libraries(test_matrix_loader PRIVATE relinow_fixture_loader)
target_compile_definitions(test_matrix_loader PRIVATE
    MATRIX_FIXTURE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/matrix_fixture.bin"
)

add_executable(test_vector_loader
    src/test_vector_loader.c
)
target_link_libraries(test_vector_loader PRIVATE relinow_fixture_loader)
target_compile_definitions(test_vector_loader PRIVATE
    VECTOR_FIXTURE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/vector_fixture.bin"
)

add_executable(test_packet_codec
    src/test_packet_codec.c
)
target_link_libraries(test_packet_codec PRIVATE relinow_core)

add_executable(test_state_core
    src/test_state_core.c
)
target_link_libraries(test_state_core PRIVATE relinow_core)

add_executable(test_reliable_mvp
    src/test_reliable_mvp.c
)
target_link_libraries(test_reliable_mvp PRIVATE relinow_core)

add_executable(test_reliable_state
    src/test_reliable_state.c
)
target_link_libraries(test_reliable_state PRIVATE relinow_core)

include(CTest)
enable_testing()

add_test(NAME relinow_test_matrix_loader COMMAND test_matrix_loader)
add_test(NAME relinow_test_vector_loader COMMAND test_vector_loader)
add_test(NAME relinow_test_packet_codec COMMAND test_packet_codec)
add_test(NAME relinow_test_state_core COMMAND test_state_core)
add_test(NAME relinow_test_reliable_mvp COMMAND test_reliable_mvp)
add_test(NAME relinow_test_reliable_state COMMAND test_reliable_state)
