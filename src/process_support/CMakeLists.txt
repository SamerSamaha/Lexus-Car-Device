add_library(lexus_head_unit_process_support STATIC
    include/lexus_head_unit/process_support/quit_on_signals.h
    source/quit_on_signals.cpp
)

target_include_directories(lexus_head_unit_process_support
    PUBLIC
        include
)

target_link_libraries(lexus_head_unit_process_support
    PUBLIC
        Qt6::Core
    PRIVATE
        lexus_head_unit_build_settings
)
