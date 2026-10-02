add_library(lexus_head_unit_service STATIC
    source/library_version.cpp
)

target_include_directories(lexus_head_unit_service
    PUBLIC
        include
)

target_link_libraries(lexus_head_unit_service
    PRIVATE
        lexus_head_unit_build_settings
)
