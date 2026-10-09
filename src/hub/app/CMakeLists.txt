qt_add_executable(lexus_head_unit_hub_app
    main.cpp
)

lexus_head_unit_add_hub_qml_module(lexus_head_unit_hub_app)

target_link_libraries(lexus_head_unit_hub_app
    PRIVATE
        lexus_head_unit_hub_viewmodels
        lexus_head_unit_hmi_viewmodels
        lexus_head_unit_service_dbus
        lexus_head_unit_hub_core
        lexus_head_unit_process_support
        lexus_head_unit_service
        lexus_head_unit_build_settings
        Qt6::DBus
        Qt6::Gui
        Qt6::Network
        Qt6::Qml
        Qt6::Quick
)

set_target_properties(lexus_head_unit_hub_app PROPERTIES OUTPUT_NAME lexus-hub)
