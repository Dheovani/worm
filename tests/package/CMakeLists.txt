cmake_minimum_required(VERSION 3.20)

project(WormPackageConsumer LANGUAGES CXX)

include(CTest)

set(CMAKE_MAP_IMPORTED_CONFIG_RELEASE Debug)
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Debug)
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL Debug)

find_package(Worm CONFIG REQUIRED)

add_executable(WormPackageConsumer main.cpp)
target_link_libraries(WormPackageConsumer PRIVATE Worm::Core)

add_test(NAME WormPackageConsumer.Runs COMMAND WormPackageConsumer)
