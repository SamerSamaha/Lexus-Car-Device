add_library(lexus_head_unit_can STATIC
    include/lexus_head_unit/hardware/can_frame.h
    include/lexus_head_unit/hardware/can_frame_reader.h
    include/lexus_head_unit/hardware/dbc_database.h
    include/lexus_head_unit/hardware/dbc_decoder.h
    include/lexus_head_unit/hardware/fake_can_frame_reader.h
    include/lexus_head_unit/hardware/socket_can_frame_reader.h
    source/dbc_database.cpp
    source/dbc_decoder.cpp
    source/fake_can_frame_reader.cpp
    source/socket_can_frame_reader.cpp
)

target_include_directories(lexus_head_unit_can
    PUBLIC
        include
)

target_link_libraries(lexus_head_unit_can
    PRIVATE
        lexus_head_unit_build_settings
)

# The CAN VehicleDataSource (DN-028). Its name ends in _source, so the link-graph check keeps it
# away from the service and HMI targets (REQ-002).
add_library(lexus_head_unit_can_source STATIC
    include/lexus_head_unit/hardware/socket_can_dbc_source.h
    source/socket_can_dbc_source.cpp
)

target_include_directories(lexus_head_unit_can_source
    PUBLIC
        include
)

target_link_libraries(lexus_head_unit_can_source
    PUBLIC
        lexus_head_unit_can
        lexus_head_unit_service
    PRIVATE
        lexus_head_unit_build_settings
)
