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
option(WORM_BUILD_CLI "Build the Worm entity and schema cli" OFF)
option(WORM_BUILD_DOCS "Add the Worm API documentation target" OFF)
option(WORM_BUILD_BENCHMARKS "Build Worm performance benchmarks" OFF)

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
if(WORM_BUILD_CLI)
  list(APPEND WORM_VCPKG_FEATURES cli)
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

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(WORM_ENABLE_STRICT_WARNINGS "Enable strict compiler warnings for Worm targets" ${PROJECT_IS_TOP_LEVEL})
option(WORM_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(WORM_ENABLE_SANITIZERS "Enable supported runtime sanitizers for Worm targets" OFF)
option(WORM_ENABLE_COVERAGE "Enable source coverage instrumentation for Worm targets" OFF)
option(WORM_BUILD_FUZZERS "Build libFuzzer targets for Worm" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WORM_ENABLE_STRICT_WARNINGS)
  if(MSVC)
    add_compile_options(/W4)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
  else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-missing-field-initializers)
  endif()
endif()

if(WORM_WARNINGS_AS_ERRORS)
  if(MSVC)
    add_compile_options(/WX)
  else()
    add_compile_options(-Werror)
  endif()
endif()

if(WORM_BUILD_FUZZERS AND (WIN32 OR NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
  message(FATAL_ERROR "WORM_BUILD_FUZZERS requires Clang with libFuzzer support on a Unix-like platform.")
endif()

if(WORM_ENABLE_SANITIZERS OR WORM_BUILD_FUZZERS)
  if(MSVC)
    add_compile_options(/fsanitize=address)
    add_link_options(/INCREMENTAL:NO)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  else()
    message(FATAL_ERROR "WORM_ENABLE_SANITIZERS is not supported by ${CMAKE_CXX_COMPILER_ID}.")
  endif()
endif()

if(WORM_ENABLE_COVERAGE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT MSVC)
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
  else()
    message(FATAL_ERROR "WORM_ENABLE_COVERAGE requires GCC or Clang on a Unix-like platform.")
  endif()
endif()

# Existing headers use project-rooted includes such as <core/model/entity.hpp>.
set(PROJECT_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/src")

include(CTest)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(dependencies)

add_subdirectory(src)

if(WORM_BUILD_CLI)
  add_subdirectory(cli)
endif()

# Keep the root ready for future test suites without requiring the directory now.
if(BUILD_TESTING AND EXISTS "${PROJECT_SOURCE_DIR}/tests/CMakeLists.txt")
  add_subdirectory(tests)
endif()

if(WORM_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(WORM_BUILD_DOCS)
  add_subdirectory(docs)
endif()

if(WORM_BUILD_FUZZERS)
  add_subdirectory(fuzz)
endif()

if(WORM_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()

set(
  WORM_INSTALL_CMAKEDIR
  "${CMAKE_INSTALL_LIBDIR}/cmake/Worm"
  CACHE STRING "Directory for Worm CMake package files"
)

install(
  TARGETS
    WormErrors
    WormReflection
    WormUtils
    WormCore
    WormConnection
    WormDependencies
  EXPORT WormTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(
  DIRECTORY "${PROJECT_SOURCE_DIR}/src/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  FILES_MATCHING PATTERN "*.hpp"
)

configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/cmake/WormConfig.cmake.in"
  "${PROJECT_BINARY_DIR}/WormConfig.cmake"
  INSTALL_DESTINATION "${WORM_INSTALL_CMAKEDIR}"
)

install(
  EXPORT WormTargets
  FILE WormTargets.cmake
  NAMESPACE Worm::
  DESTINATION "${WORM_INSTALL_CMAKEDIR}"
)

install(
  FILES "${PROJECT_BINARY_DIR}/WormConfig.cmake"
  DESTINATION "${WORM_INSTALL_CMAKEDIR}"
)

install(
  FILES "${PROJECT_SOURCE_DIR}/LICENSE"
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/Worm"
)

if(WORM_BUILD_CLI)
  install(TARGETS WormCli RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
