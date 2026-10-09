add_library(lexus_head_unit_can STATIC
    include/lexus_head_unit/hardware/can_frame.h
    include/lexus_head_unit/hardware/dbc_database.h
    include/lexus_head_unit/hardware/dbc_decoder.h
    source/dbc_database.cpp
    source/dbc_decoder.cpp
)

target_include_directories(lexus_head_unit_can
    PUBLIC
        include
)

target_link_libraries(lexus_head_unit_can
    PRIVATE
        lexus_head_unit_build_settings
)
