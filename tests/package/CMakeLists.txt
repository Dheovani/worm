cmake_minimum_required(VERSION 3.20)

project(WormPackageConsumer LANGUAGES CXX)

include(CTest)

if(NOT DEFINED WORM_PACKAGE_CONFIG OR WORM_PACKAGE_CONFIG STREQUAL "")
  message(FATAL_ERROR "WORM_PACKAGE_CONFIG must identify the installed Worm configuration.")
endif()

find_package(Worm ${WORM_PACKAGE_VERSION_REQUEST} CONFIG REQUIRED)
if(NOT Worm_VERSION STREQUAL WORM_PACKAGE_EXPECTED_VERSION)
  message(FATAL_ERROR "Expected installed version '${WORM_PACKAGE_EXPECTED_VERSION}', got '${Worm_VERSION}'.")
endif()

foreach(wormTarget IN ITEMS Worm::Utils Worm::Connection Worm::Core)
  set_target_properties(
    ${wormTarget}
    PROPERTIES
      MAP_IMPORTED_CONFIG_DEBUG "${WORM_PACKAGE_CONFIG}"
      MAP_IMPORTED_CONFIG_RELEASE "${WORM_PACKAGE_CONFIG}"
      MAP_IMPORTED_CONFIG_RELWITHDEBINFO "${WORM_PACKAGE_CONFIG}"
      MAP_IMPORTED_CONFIG_MINSIZEREL "${WORM_PACKAGE_CONFIG}")
endforeach()

add_executable(WormPackageConsumer main.cpp)
target_link_libraries(WormPackageConsumer PRIVATE Worm::Core)

add_test(NAME WormPackageConsumer.Runs COMMAND WormPackageConsumer)
