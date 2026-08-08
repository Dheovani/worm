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

option(WORM_ENABLE_POSTGRESQL "Build the PostgreSQL driver" ON)
option(WORM_ENABLE_MYSQL "Build the MySQL driver" ON)
option(WORM_ENABLE_SQLITE "Build the SQLite driver" ON)
option(WORM_ENABLE_MSSQL "Build the Microsoft SQL Server driver" OFF)
option(WORM_BUILD_EXAMPLES "Build Worm usage examples" OFF)

set(WORM_VCPKG_FEATURES)
if(WORM_ENABLE_POSTGRESQL)
  list(APPEND WORM_VCPKG_FEATURES postgresql)
endif()
if(WORM_ENABLE_MYSQL)
  list(APPEND WORM_VCPKG_FEATURES mysql)
endif()
if(WORM_ENABLE_SQLITE)
  list(APPEND WORM_VCPKG_FEATURES sqlite)
endif()
if(WORM_ENABLE_MSSQL)
  list(APPEND WORM_VCPKG_FEATURES mssql)
endif()

# Keep manifest dependencies aligned with the selected drivers. FORCE is
# intentional so toggling an option in an existing build directory cannot
# leave a stale vcpkg feature enabled.
set(
  VCPKG_MANIFEST_FEATURES
  "${WORM_VCPKG_FEATURES}"
  CACHE STRING "vcpkg features selected by Worm driver options"
  FORCE
)
set(
  VCPKG_MANIFEST_NO_DEFAULT_FEATURES
  ON
  CACHE BOOL "Use only the vcpkg features selected by Worm"
  FORCE
)

project(
  worm
  VERSION 0.1.0
  DESCRIPTION "A C++ ORM inspired by Doctrine"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Existing headers use project-rooted includes such as <core/model/entity.hpp>.
set(PROJECT_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/src")

include(CTest)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(dependencies)

add_subdirectory(src)

# Keep the root ready for future test suites without requiring the directory now.
if(BUILD_TESTING AND EXISTS "${PROJECT_SOURCE_DIR}/tests/CMakeLists.txt")
  add_subdirectory(tests)
endif()

if(WORM_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
