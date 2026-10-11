add_library(lexus_head_unit_phone_link STATIC
    include/lexus_head_unit/phone_link/http_request.h
    include/lexus_head_unit/phone_link/phone_link_server.h
    include/lexus_head_unit/phone_link/phone_position.h
    source/http_request.cpp
    source/phone_link_server.cpp
    source/phone_position.cpp
)

set_target_properties(lexus_head_unit_phone_link PROPERTIES AUTOMOC ON)

qt_add_resources(lexus_head_unit_phone_link phone_link_page
    PREFIX /phone_link
    BASE resources
    FILES resources/phone_page.html
)

target_include_directories(lexus_head_unit_phone_link
    PUBLIC
        include
)

target_link_libraries(lexus_head_unit_phone_link
    PUBLIC
        lexus_head_unit_service
        Qt6::Core
        Qt6::Network
    PRIVATE
        lexus_head_unit_build_settings
)
