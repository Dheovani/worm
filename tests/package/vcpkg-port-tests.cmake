cmake_minimum_required(VERSION 3.20)

foreach(requiredVariable IN ITEMS TEST_ROOT SOURCE_ROOT)
  if(NOT DEFINED ${requiredVariable})
    message(FATAL_ERROR "${requiredVariable} is required.")
  endif()
  if("${${requiredVariable}}" STREQUAL "")
    message(FATAL_ERROR "${requiredVariable} is required.")
  endif()
endforeach()

include("${SOURCE_ROOT}/cmake/version.cmake")
worm_read_version("${SOURCE_ROOT}/vcpkg.json")

file(REMOVE_RECURSE "${TEST_ROOT}")

set(localPort "${TEST_ROOT}/local")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DWORM_SOURCE_ROOT=${SOURCE_ROOT}"
    "-DWORM_OUTPUT_DIR=${localPort}"
    "-DWORM_LOCAL_SOURCE_PATH=${SOURCE_ROOT}"
    -P "${SOURCE_ROOT}/cmake/prepare-vcpkg-port.cmake"
  RESULT_VARIABLE localResult
  OUTPUT_VARIABLE localOutput
  ERROR_VARIABLE localError
)
if(NOT localResult EQUAL 0)
  message(FATAL_ERROR "Local-source port generation failed: ${localOutput}\n${localError}")
endif()

file(READ "${localPort}/worm/portfile.cmake" localPortfile)
file(READ "${localPort}/worm/vcpkg.json" portManifest)
if(NOT localPortfile MATCHES "set\\(SOURCE_PATH \\[\\[")
  message(FATAL_ERROR "Local-source port does not contain the source-tree override.")
endif()
if(NOT portManifest MATCHES "\\\"version-semver\\\"[ \t]*:[ \t]*\\\"${WORM_VERSION}\\\"")
  message(FATAL_ERROR "Generated port version does not match the source manifest.")
endif()

string(REPEAT "a" 128 validHash)
set(taggedPort "${TEST_ROOT}/tagged")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DWORM_SOURCE_ROOT=${SOURCE_ROOT}"
    "-DWORM_OUTPUT_DIR=${taggedPort}"
    "-DWORM_SOURCE_SHA512=${validHash}"
    -P "${SOURCE_ROOT}/cmake/prepare-vcpkg-port.cmake"
  RESULT_VARIABLE taggedResult
  OUTPUT_VARIABLE taggedOutput
  ERROR_VARIABLE taggedError
)
if(NOT taggedResult EQUAL 0)
  message(FATAL_ERROR "Tagged-source port generation failed: ${taggedOutput}\n${taggedError}")
endif()

file(READ "${taggedPort}/worm/portfile.cmake" taggedPortfile)
string(FIND "${taggedPortfile}" "REF \"v\${VERSION}\"" tagReferencePosition)
if(tagReferencePosition EQUAL -1)
  message(FATAL_ERROR "Tagged-source port does not fetch its matching immutable release tag.")
endif()
if(NOT taggedPortfile MATCHES "SHA512 ${validHash}")
  message(FATAL_ERROR "Tagged-source port does not contain the supplied SHA-512 digest.")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DWORM_SOURCE_ROOT=${SOURCE_ROOT}"
    "-DWORM_OUTPUT_DIR=${TEST_ROOT}/invalid"
    "-DWORM_SOURCE_SHA512=invalid"
    -P "${SOURCE_ROOT}/cmake/prepare-vcpkg-port.cmake"
  RESULT_VARIABLE invalidResult
  OUTPUT_QUIET
  ERROR_QUIET
)
if(invalidResult EQUAL 0)
  message(FATAL_ERROR "Port generation accepted an invalid SHA-512 digest.")
endif()
