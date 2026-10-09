add_library(lexus_head_unit_obd STATIC
    source/command_allowlist.cpp
    source/elm327_protocol.cpp
    source/hex.cpp
    source/obd_pid_decoder.cpp
    source/supported_pid_set.cpp
)

target_include_directories(lexus_head_unit_obd
    PUBLIC
        include
)

target_link_libraries(lexus_head_unit_obd
    PUBLIC
        lexus_head_unit_service
        lexus_head_unit_transport
    PRIVATE
        lexus_head_unit_build_settings
)
