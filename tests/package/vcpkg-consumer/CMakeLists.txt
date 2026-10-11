cmake_minimum_required(VERSION 3.20)

project(WormVcpkgConsumer LANGUAGES CXX)

include(CTest)

if(NOT DEFINED WORM_PORT_MANIFEST OR WORM_PORT_MANIFEST STREQUAL "")
  message(FATAL_ERROR "WORM_PORT_MANIFEST must identify the generated Worm port manifest.")
endif()
file(READ "${WORM_PORT_MANIFEST}" wormPortManifest)
string(JSON wormExpectedVersion GET "${wormPortManifest}" version-semver)

find_package(Worm CONFIG REQUIRED)
if(NOT Worm_VERSION STREQUAL wormExpectedVersion)
  message(FATAL_ERROR "Expected Worm ${wormExpectedVersion}, got ${Worm_VERSION}.")
endif()

add_executable(WormVcpkgConsumer main.cpp)
target_compile_features(WormVcpkgConsumer PRIVATE cxx_std_20)
target_link_libraries(WormVcpkgConsumer PRIVATE Worm::Connection Worm::Core)

add_test(NAME WormVcpkgConsumer.Runs COMMAND WormVcpkgConsumer)
