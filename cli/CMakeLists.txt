set(WORM_CLI_ENTRYPOINT "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

# The cli is configured before its implementation is introduced. Keep
# configuration with WORM_BUILD_CLI=ON valid, then create the CLI target
# automatically once its entry point exists.
if(NOT EXISTS "${WORM_CLI_ENTRYPOINT}")
  message(STATUS "Worm cli configured; CLI target awaits cli/src/main.cpp")
  return()
endif()

add_library(
  WormCliCore
  STATIC
    src/parser.cpp
    src/validator.cpp
)

target_compile_features(WormCliCore PUBLIC cxx_std_20)
target_include_directories(
  WormCliCore
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(WormCliCore PUBLIC Worm::Errors)

add_executable(WormCli "${WORM_CLI_ENTRYPOINT}")
add_executable(Worm::Cli ALIAS WormCli)

set_target_properties(WormCli PROPERTIES OUTPUT_NAME worm)

target_compile_features(WormCli PRIVATE cxx_std_20)
target_link_libraries(
  WormCli
  PRIVATE
    Worm::Connection
    Worm::Core
    WormCliCore
)
