# Adds the LexusHub QML module to a target (the hub or its HMI test executable). Sizes.qml is
# the vehicle-data app's file, so that both use the same millimetre rules (D-024).
function(lexus_head_unit_add_hub_qml_module target)
    set(qml_files
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/AppTile.qml
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/HubScreen.qml
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/HubStatusStrip.qml
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Main.qml
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../hmi/qml/Sizes.qml
    )
    foreach(qml_file IN LISTS qml_files)
        get_filename_component(qml_name "${qml_file}" NAME)
        set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_name}")
    endforeach()
    set_source_files_properties(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../hmi/qml/Sizes.qml
        PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
    qt_add_qml_module(${target}
        URI LexusHub
        VERSION 1.0
        RESOURCE_PREFIX /
        NO_PLUGIN
        NO_CACHEGEN
        QML_FILES ${qml_files}
    )
endfunction()
