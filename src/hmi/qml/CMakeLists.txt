# Adds the LexusHeadUnit QML module to a target (the application or an HMI test executable).
# The module is compiled into each executable that calls this, from the same files.
function(lexus_head_unit_add_qml_module target)
    set(qml_files
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/HomeScreen.qml
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Main.qml
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SignalTile.qml
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Sizes.qml
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/StatusStrip.qml
    )
    foreach(qml_file IN LISTS qml_files)
        get_filename_component(qml_name "${qml_file}" NAME)
        set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_name}")
    endforeach()
    set_source_files_properties(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Sizes.qml
        PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
    qt_add_qml_module(${target}
        URI LexusHeadUnit
        VERSION 1.0
        RESOURCE_PREFIX /
        NO_PLUGIN
        NO_CACHEGEN
        QML_FILES ${qml_files}
    )
endfunction()
