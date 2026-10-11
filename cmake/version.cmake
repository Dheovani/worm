include_guard(GLOBAL)

# The manifest is the single release-version source, including prerelease suffixes.
function(worm_read_version manifest)
  file(READ "${manifest}" manifestContent)
  string(JSON releaseVersion GET "${manifestContent}" version-semver)
  set(number "(0|[1-9][0-9]*)")
  if(NOT releaseVersion MATCHES "^${number}\\.${number}\\.${number}(-(alpha|beta|rc)\\.${number})?$")
    message(FATAL_ERROR "Invalid Worm release version '${releaseVersion}'; expected X.Y.Z or X.Y.Z-alpha.N/beta.N/rc.N without leading zeroes.")
  endif()
  set(WORM_VERSION "${releaseVersion}" PARENT_SCOPE)
  set(WORM_VERSION_NUMERIC "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}" PARENT_SCOPE)
  set(WORM_VERSION_MAJOR "${CMAKE_MATCH_1}" PARENT_SCOPE)
  set(WORM_VERSION_PRERELEASE "${CMAKE_MATCH_4}" PARENT_SCOPE)
endfunction()

function(worm_write_package_version output)
  include(CMakePackageConfigHelpers)
  if(WORM_VERSION_MAJOR EQUAL 0)
    set(compatibility ExactVersion)
  else()
    set(compatibility SameMajorVersion)
  endif()
  write_basic_package_version_file("${output}"
    VERSION "${WORM_VERSION_NUMERIC}" COMPATIBILITY "${compatibility}")
  file(READ "${output}" WORM_NUMERIC_VERSION_CHECK)
  # The standard helper can return early (e.g. a consumer without a language).
  # Keep its version assignment full so those returns preserve prerelease identity.
  string(REPLACE "set(PACKAGE_VERSION \"${WORM_VERSION_NUMERIC}\")"
    "set(PACKAGE_VERSION \"${WORM_VERSION}\")" WORM_NUMERIC_VERSION_CHECK "${WORM_NUMERIC_VERSION_CHECK}")
  configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/WormConfigVersion.cmake.in" "${output}" @ONLY)
endfunction()
