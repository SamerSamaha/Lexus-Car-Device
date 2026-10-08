cmake_minimum_required(VERSION 3.25)

project(LexusHeadUnit
    VERSION 0.1.0
    DESCRIPTION "Read-only infotainment head unit for a 2013 Lexus GS350"
    LANGUAGES CXX
)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not supported. Use a preset: cmake --preset debug")
endif()

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
    message(FATAL_ERROR
        "Unsupported compiler '${CMAKE_CXX_COMPILER_ID}'. Use g++ or clang++ on Linux.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LEXUS_HEAD_UNIT_WARNINGS_AS_ERRORS
    "Treat every compiler warning as an error" ON)
option(LEXUS_HEAD_UNIT_ENABLE_SANITIZERS
    "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(BUILD_TESTING
    "Build the unit tests" ON)

add_library(lexus_head_unit_build_settings INTERFACE)

target_compile_options(lexus_head_unit_build_settings INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Wsign-conversion
    -Wold-style-cast
    -Wnon-virtual-dtor
    -Woverloaded-virtual
)

if(LEXUS_HEAD_UNIT_WARNINGS_AS_ERRORS)
    target_compile_options(lexus_head_unit_build_settings INTERFACE -Werror)
endif()

if(LEXUS_HEAD_UNIT_ENABLE_SANITIZERS)
    target_compile_options(lexus_head_unit_build_settings INTERFACE
        -fsanitize=address,undefined
        -fno-sanitize-recover=undefined
        -fno-omit-frame-pointer
    )
    target_link_options(lexus_head_unit_build_settings INTERFACE
        -fsanitize=address,undefined
    )
endif()

add_subdirectory(src/service)
add_subdirectory(src/hardware)

if(BUILD_TESTING)
    enable_testing()
    find_package(GTest REQUIRED)
    add_subdirectory(tests/unit)
    add_subdirectory(tests/integration)
endif()
