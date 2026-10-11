add_library(lexus_head_unit_hmi_map STATIC
    include/lexus_head_unit/hmi/map_view_model.h
    source/map_view_model.cpp
)

set_target_properties(lexus_head_unit_hmi_map PROPERTIES AUTOMOC ON)

target_include_directories(lexus_head_unit_hmi_map
    PUBLIC
        include
)

target_link_libraries(lexus_head_unit_hmi_map
    PUBLIC
        lexus_head_unit_qt_value_types
        lexus_head_unit_service
        Qt6::Core
    PRIVATE
        lexus_head_unit_build_settings
)

# Adds the LexusMap QML module to a target (the map app or its HMI test executable). Sizes.qml
# is the vehicle-data app's file, so that the map uses the same millimetre rules (D-024).
# QtLocation and QtPositioning are imported at run time only, by MapView.qml through a Loader,
# so no Qt location library is needed to build or to test the rest of the screen.
function(lexus_head_unit_add_map_qml_module target)
    set(qml_files
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/qml/Main.qml
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/qml/MapScreen.qml
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/qml/MapView.qml
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../qml/Sizes.qml
    )
    foreach(qml_file IN LISTS qml_files)
        get_filename_component(qml_name "${qml_file}" NAME)
        set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_name}")
    endforeach()
    set_source_files_properties(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../qml/Sizes.qml
        PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
    qt_add_qml_module(${target}
        URI LexusMap
        VERSION 1.0
        RESOURCE_PREFIX /qt/qml
        NO_PLUGIN
        NO_CACHEGEN
        QML_FILES ${qml_files}
    )
endfunction()
