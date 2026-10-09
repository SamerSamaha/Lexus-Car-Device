add_library(lexus_head_unit_hub_core STATIC
    include/lexus_head_unit/hub/app_entry.h
    include/lexus_head_unit/hub/app_process_manager.h
    include/lexus_head_unit/hub/app_registry.h
    include/lexus_head_unit/hub/command_line.h
    include/lexus_head_unit/hub/posix_process_launcher.h
    include/lexus_head_unit/hub/process_launcher.h
    source/app_entry.cpp
    source/app_process_manager.cpp
    source/app_registry.cpp
    source/command_line.cpp
    source/posix_process_launcher.cpp
)

target_include_directories(lexus_head_unit_hub_core
    PUBLIC
        include
)

target_link_libraries(lexus_head_unit_hub_core
    PUBLIC
        lexus_head_unit_service
    PRIVATE
        lexus_head_unit_build_settings
)
