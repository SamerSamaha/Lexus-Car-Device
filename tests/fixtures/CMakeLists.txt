add_executable(lexus_head_unit_fake_app
    fake_app.cpp
)

target_link_libraries(lexus_head_unit_fake_app
    PRIVATE
        lexus_head_unit_build_settings
)
