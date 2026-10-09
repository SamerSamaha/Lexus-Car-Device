add_library(lexus_head_unit_hmi_viewmodels STATIC
    include/lexus_head_unit/hmi/connection_status_model.h
    include/lexus_head_unit/hmi/signal_tile_model.h
    include/lexus_head_unit/hmi/value_types.h
    include/lexus_head_unit/hmi/vehicle_data_view_model.h
    include/lexus_head_unit/hmi/worker_bridge.h
    source/connection_status_model.cpp
    source/signal_tile_model.cpp
    source/vehicle_data_view_model.cpp
    source/worker_bridge.cpp
)

set_target_properties(lexus_head_unit_hmi_viewmodels PROPERTIES AUTOMOC ON)

target_include_directories(lexus_head_unit_hmi_viewmodels
    PUBLIC
        include
)

target_link_libraries(lexus_head_unit_hmi_viewmodels
    PUBLIC
        lexus_head_unit_service
        Qt6::Core
    PRIVATE
        lexus_head_unit_build_settings
)
