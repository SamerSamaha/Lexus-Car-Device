# Recording on the byte transport and playback (DN-029, REQ-015).
add_library(lexus_head_unit_recording STATIC
    include/lexus_head_unit/hardware/recording.h
    source/recording.cpp
)

target_include_directories(lexus_head_unit_recording
    PUBLIC
        include
)

target_link_libraries(lexus_head_unit_recording
    PUBLIC
        lexus_head_unit_transport
        lexus_head_unit_service
    PRIVATE
        lexus_head_unit_build_settings
)

add_library(lexus_head_unit_replay_source STATIC
    include/lexus_head_unit/hardware/replay_source.h
    source/replay_source.cpp
)

target_include_directories(lexus_head_unit_replay_source
    PUBLIC
        include
)

target_link_libraries(lexus_head_unit_replay_source
    PUBLIC
        lexus_head_unit_recording
        lexus_head_unit_elm327_source
    PRIVATE
        lexus_head_unit_build_settings
)
