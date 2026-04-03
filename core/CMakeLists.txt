idf_component_register(
    SRCS
        "src/relinow_packet.c"
        "src/relinow_state.c"
        "src/relinow_reliable.c"
        "src/relinow_espnow.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_event
)
