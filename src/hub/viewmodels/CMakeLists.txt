add_library(lexus_head_unit_hub_viewmodels STATIC
    include/lexus_head_unit/hub/hub_control_server.h
    include/lexus_head_unit/hub/hub_view_model.h
    include/lexus_head_unit/hub/power_status_model.h
    source/hub_control_server.cpp
    source/hub_view_model.cpp
    source/power_status_model.cpp
)

set_target_properties(lexus_head_unit_hub_viewmodels PROPERTIES AUTOMOC ON)

target_include_directories(lexus_head_unit_hub_viewmodels
    PUBLIC
        include
)

target_link_libraries(lexus_head_unit_hub_viewmodels
    PUBLIC
        lexus_head_unit_hub_core
        Qt6::Core
        Qt6::Network
    PRIVATE
        lexus_head_unit_build_settings
)
