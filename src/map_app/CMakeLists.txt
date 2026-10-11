qt_add_executable(lexus_head_unit_map_app
    main.cpp
)

lexus_head_unit_add_map_qml_module(lexus_head_unit_map_app)

target_link_libraries(lexus_head_unit_map_app
    PRIVATE
        lexus_head_unit_hmi_map
        lexus_head_unit_hub_viewmodels
        lexus_head_unit_service_dbus
        lexus_head_unit_service
        lexus_head_unit_build_settings
        Qt6::DBus
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
)

set_target_properties(lexus_head_unit_map_app PROPERTIES OUTPUT_NAME lexus-map)
