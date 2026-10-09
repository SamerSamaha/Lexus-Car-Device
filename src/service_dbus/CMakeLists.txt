add_library(lexus_head_unit_service_dbus STATIC
    include/lexus_head_unit/service_dbus/dbus_names.h
    include/lexus_head_unit/service_dbus/dbus_types.h
    include/lexus_head_unit/service_dbus/vehicle_data_client.h
    include/lexus_head_unit/service_dbus/vehicle_data_service.h
    source/dbus_types.cpp
    source/vehicle_data_client.cpp
    source/vehicle_data_service.cpp
)

set_target_properties(lexus_head_unit_service_dbus PROPERTIES AUTOMOC ON)

target_include_directories(lexus_head_unit_service_dbus
    PUBLIC
        include
)

target_link_libraries(lexus_head_unit_service_dbus
    PUBLIC
        lexus_head_unit_qt_value_types
        lexus_head_unit_service
        Qt6::Core
        Qt6::DBus
    PRIVATE
        lexus_head_unit_build_settings
)

qt_add_executable(lexus_head_unit_vehicle_data_service_app
    app/main.cpp
)

target_link_libraries(lexus_head_unit_vehicle_data_service_app
    PRIVATE
        lexus_head_unit_service_dbus
        lexus_head_unit_source_wiring
        lexus_head_unit_process_support
        lexus_head_unit_service
        lexus_head_unit_build_settings
        Qt6::Core
        Qt6::DBus
)

set_target_properties(lexus_head_unit_vehicle_data_service_app
    PROPERTIES OUTPUT_NAME lexus-vehicle-data-service)
