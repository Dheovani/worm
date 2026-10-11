cmake_minimum_required(VERSION 3.20)
include("${CMAKE_CURRENT_LIST_DIR}/version.cmake")
worm_read_version("${CMAKE_CURRENT_LIST_DIR}/../vcpkg.json")

if(NOT DEFINED WORM_RELEASE_TAG OR NOT WORM_RELEASE_TAG STREQUAL "v${WORM_VERSION}")
  message(FATAL_ERROR "Release tag must be 'v${WORM_VERSION}', got '${WORM_RELEASE_TAG}'.")
endif()

if(DEFINED WORM_ARTIFACT_VERSION_FILE)
  file(READ "${WORM_ARTIFACT_VERSION_FILE}" artifactVersion)
  string(STRIP "${artifactVersion}" artifactVersion)
  if(NOT artifactVersion STREQUAL WORM_VERSION)
    message(FATAL_ERROR "Artifact version '${artifactVersion}' differs from manifest '${WORM_VERSION}'.")
  endif()
endif()

if(DEFINED WORM_CLI_EXECUTABLE)
  execute_process(COMMAND "${WORM_CLI_EXECUTABLE}" --version
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT result STREQUAL "0" OR NOT output STREQUAL "worm ${WORM_VERSION}")
    message(FATAL_ERROR "CLI version mismatch: '${output}' (exit ${result}). ${error}")
  endif()
endif()
message(STATUS "Worm release version verified: ${WORM_VERSION}")
