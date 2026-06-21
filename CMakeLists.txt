cmake_minimum_required(VERSION 3.20)

# Enable vcpkg manifest mode automatically when VCPKG_ROOT is available.
# This must be set before the first project() call.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(
        CMAKE_TOOLCHAIN_FILE
        "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE FILEPATH "vcpkg toolchain file"
    )
endif()

project(
    worm
    VERSION 0.1.0
    DESCRIPTION "A C++ ORM inspired by Doctrine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Existing headers use project-rooted includes such as <core/entity.hpp>.
set(PROJECT_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/src")

option(WORM_BUILD_APP "Build the Worm example executable" ON)

include(CTest)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(dependencies)

add_subdirectory(src)

# Keep the root ready for future test suites without requiring the directory now.
if(BUILD_TESTING AND EXISTS "${PROJECT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
