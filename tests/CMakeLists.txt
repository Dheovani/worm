add_subdirectory(errors)
add_subdirectory(connection)
add_subdirectory(utils)
add_subdirectory(reflection)
add_subdirectory(core)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/context/CMakeLists.txt")
  add_subdirectory(context)
endif()
