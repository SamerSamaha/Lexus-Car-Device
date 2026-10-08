add_library(lexus_head_unit_transport STATIC
    source/fake_byte_transport.cpp
)

target_include_directories(lexus_head_unit_transport
    PUBLIC
        include
)

target_link_libraries(lexus_head_unit_transport
    PRIVATE
        lexus_head_unit_build_settings
)
