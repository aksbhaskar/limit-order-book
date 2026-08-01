cmake_minimum_required(VERSION 3.20)

project(limit_order_book
    VERSION 0.1.0
    DESCRIPTION "High-performance limit order book and matching engine"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Global settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to a Release build when the user does not specify one.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Export a compile database for editors/tooling.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Only the top-level project drives testing and warnings-as-first-class-citizens.
set(LOB_IS_TOP_LEVEL OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(LOB_IS_TOP_LEVEL ON)
endif()

option(LOB_BUILD_TESTS "Build unit tests" ${LOB_IS_TOP_LEVEL})

# Shared warning flags for our own targets.
add_library(lob_warnings INTERFACE)
if(MSVC)
    target_compile_options(lob_warnings INTERFACE /W4 /permissive-)
else()
    target_compile_options(lob_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------
add_subdirectory(src)

if(LOB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
