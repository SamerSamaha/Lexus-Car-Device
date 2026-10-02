# Top-level build file for the Lexus Head Unit.
#
# Configure, build and test through the presets in CMakePresets.json:
#     cmake --preset debug
#     cmake --build --preset debug
#     ctest --preset debug

cmake_minimum_required(VERSION 3.25)

project(LexusHeadUnit
    VERSION 0.1.0
    DESCRIPTION "Read-only infotainment head unit for a 2013 Lexus GS350"
    LANGUAGES CXX
)

# Build output never goes into the source tree. The source folder is on the
# Windows drive and the build folder is inside the Linux home folder.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not supported. Use a preset: cmake --preset debug")
endif()

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
    message(FATAL_ERROR
        "Unsupported compiler '${CMAKE_CXX_COMPILER_ID}'. Use g++ or clang++ on Linux.")
endif()

# C++17 exactly, without compiler-specific language extensions.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# clang-tidy and editors read compile_commands.json from the build folder.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LEXUS_HEAD_UNIT_WARNINGS_AS_ERRORS
    "Treat every compiler warning as an error" ON)
option(LEXUS_HEAD_UNIT_ENABLE_SANITIZERS
    "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(BUILD_TESTING
    "Build the unit tests" ON)

# ---------------------------------------------------------------------------
# Build settings shared by every target of this project.
#
# This is an INTERFACE library: it compiles nothing itself. A target that links
# to it with PRIVATE receives these warning and sanitizer flags, so the flags
# are written once, here, and never leak into code outside this project.
# ---------------------------------------------------------------------------
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
    # -fno-sanitize-recover makes the first undefined-behaviour report end the
    # program with a failure, so a test cannot pass while reporting one.
    target_compile_options(lexus_head_unit_build_settings INTERFACE
        -fsanitize=address,undefined
        -fno-sanitize-recover=undefined
        -fno-omit-frame-pointer
    )
    target_link_options(lexus_head_unit_build_settings INTERFACE
        -fsanitize=address,undefined
    )
endif()

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------

# Service layer: plain C++17, no Qt.
# Defines the library target lexus_head_unit_service.
add_subdirectory(src/service)

if(BUILD_TESTING)
    # enable_testing() must be called here, in the top-level file, so that
    # ctest finds the tests from the root of the build folder.
    enable_testing()
    find_package(GTest REQUIRED)
    add_subdirectory(tests/unit)
endif()
