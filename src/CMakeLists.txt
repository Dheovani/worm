add_subdirectory(errors)
add_subdirectory(connection)
add_subdirectory(utils)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/reflection/CMakeLists.txt")
    add_subdirectory(reflection)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/core/CMakeLists.txt")
    add_subdirectory(core)
endif()

if(WORM_BUILD_APP AND TARGET Worm::Core)
    add_executable(worm main.cpp)
    target_link_libraries(worm PRIVATE Worm::Core)
endif()
