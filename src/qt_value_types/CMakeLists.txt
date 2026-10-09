add_library(lexus_head_unit_qt_value_types INTERFACE)

target_include_directories(lexus_head_unit_qt_value_types
    INTERFACE
        include
)

target_link_libraries(lexus_head_unit_qt_value_types
    INTERFACE
        lexus_head_unit_service
        Qt6::Core
)
