add_library(lexus_head_unit_fake_source STATIC
    source/fake_source.cpp
)

target_include_directories(lexus_head_unit_fake_source
    PUBLIC
        include
)

target_link_libraries(lexus_head_unit_fake_source
    PUBLIC
        lexus_head_unit_service
    PRIVATE
        lexus_head_unit_build_settings
)
