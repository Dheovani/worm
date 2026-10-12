if(NOT DEFINED PORT_MANIFEST OR PORT_MANIFEST STREQUAL "")
  message(FATAL_ERROR "PORT_MANIFEST is required.")
endif()

file(READ "${PORT_MANIFEST}" portManifest)
string(JSON portVersion GET "${portManifest}" version-semver)
execute_process(COMMAND "${CMAKE_COMMAND}" -E echo_append "${portVersion}")
