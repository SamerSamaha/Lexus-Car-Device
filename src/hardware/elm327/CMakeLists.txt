add_library(lexus_head_unit_elm327_source STATIC
    source/elm327_obd_source.cpp
    source/elm327_source_configuration.cpp
    source/reconnect_backoff.cpp
)

target_include_directories(lexus_head_unit_elm327_source
    PUBLIC
        include
)

target_link_libraries(lexus_head_unit_elm327_source
    PUBLIC
        lexus_head_unit_obd
        lexus_head_unit_service
        lexus_head_unit_transport
    PRIVATE
        lexus_head_unit_build_settings
)
