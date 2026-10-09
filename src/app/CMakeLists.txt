# The one place that knows every concrete source; shared by the vehicle-data app and the
# vehicle-data service process (DN-022).
add_library(lexus_head_unit_source_wiring STATIC
    source_factory.cpp
    source_factory.h
)

target_include_directories(lexus_head_unit_source_wiring
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(lexus_head_unit_source_wiring
    PUBLIC
        lexus_head_unit_elm327_source
        lexus_head_unit_fake_source
        lexus_head_unit_service
    PRIVATE
        lexus_head_unit_build_settings
)

qt_add_executable(lexus_head_unit_vehicle_data_app
    main.cpp
)

lexus_head_unit_add_qml_module(lexus_head_unit_vehicle_data_app)

target_link_libraries(lexus_head_unit_vehicle_data_app
    PRIVATE
        lexus_head_unit_hmi_viewmodels
        lexus_head_unit_service_dbus
        lexus_head_unit_source_wiring
        lexus_head_unit_elm327_source
        lexus_head_unit_fake_source
        lexus_head_unit_service
        lexus_head_unit_build_settings
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
)

set_target_properties(lexus_head_unit_vehicle_data_app PROPERTIES OUTPUT_NAME lexus-head-unit)
