find_package(Doxygen QUIET)

if(NOT Doxygen_FOUND)
  message(
    FATAL_ERROR
    "WORM_BUILD_DOCS is enabled, but Doxygen was not found. Install Doxygen, "
    "open a new terminal so PATH is refreshed, and configure the build again."
  )
endif()

set(WORM_DOXYGEN_OUTPUT_DIR "${PROJECT_BINARY_DIR}/api")
set(WORM_DOXYGEN_CONFIG "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile")

configure_file(Doxyfile.in "${WORM_DOXYGEN_CONFIG}" @ONLY)

add_custom_target(
  WormDocs
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${WORM_DOXYGEN_OUTPUT_DIR}"
  COMMAND "${DOXYGEN_EXECUTABLE}" "${WORM_DOXYGEN_CONFIG}"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Generating Worm API documentation with Doxygen"
  VERBATIM
)
