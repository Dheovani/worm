if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/core/CMakeLists.txt")
  add_subdirectory(core)
endif()

add_subdirectory(errors)
add_subdirectory(connection)
add_subdirectory(utils)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/reflection/CMakeLists.txt")
  add_subdirectory(reflection)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/context/CMakeLists.txt")
  add_subdirectory(context)
endif()
