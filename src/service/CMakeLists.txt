add_library(lexus_head_unit_service STATIC
    source/clock.cpp
    source/key_value_configuration.cpp
    source/connection_state_machine.cpp
    source/library_version.cpp
    source/manual_clock.cpp
    source/reconnect_backoff.cpp
    source/signal_definition.cpp
    source/signal_id.cpp
    source/signal_store.cpp
    source/signal_store_feeder.cpp
    source/staleness_monitor.cpp
    source/worker_loop.cpp
)

target_include_directories(lexus_head_unit_service
    PUBLIC
        include
)

target_link_libraries(lexus_head_unit_service
    PUBLIC
        Threads::Threads
    PRIVATE
        lexus_head_unit_build_settings
)
