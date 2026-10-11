add_subdirectory(errors)
add_subdirectory(connection)
add_subdirectory(utils)
add_subdirectory(reflection)
add_subdirectory(core)

file(TO_CMAKE_PATH "${CMAKE_TOOLCHAIN_FILE}" WORM_PACKAGE_TEST_TOOLCHAIN_FILE)
file(TO_CMAKE_PATH "${VCPKG_INSTALLED_DIR}" WORM_PACKAGE_TEST_VCPKG_INSTALLED_DIR)
configure_file(
  package/find-package-test.cmake.in
  "${PROJECT_BINARY_DIR}/tests/find-package-test.cmake"
  @ONLY
)

add_test(
  NAME Worm.Package.FindPackage
  COMMAND
    "${CMAKE_COMMAND}"
    "-DTEST_CONFIG=$<CONFIG>"
    -P "${PROJECT_BINARY_DIR}/tests/find-package-test.cmake"
)
set_tests_properties(Worm.Package.FindPackage PROPERTIES LABELS "package;integration")

add_test(NAME Worm.Package.Version
  COMMAND "${CMAKE_COMMAND}" "-DTEST_ROOT=${PROJECT_BINARY_DIR}/version-test"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/package/version-tests.cmake")
set_tests_properties(Worm.Package.Version PROPERTIES LABELS "package;unit")

if(TARGET WormCliCore)
  add_subdirectory(cli)
endif()
