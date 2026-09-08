idf_component_register(
    SRCS 
        "../../../../core/src/relinow_packet.c"
        "../../../../core/src/relinow_state.c"
        "../../../../core/src/relinow_reliable.c"
        "../../../../core/src/relinow_espnow.c"
    INCLUDE_DIRS 
        "../../../../core/include"
    REQUIRES esp_wifi
)

