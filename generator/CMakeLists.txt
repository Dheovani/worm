set(WORM_GENERATOR_ENTRYPOINT "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

# The generator is configured before its implementation is introduced. Keep
# configuration with WORM_BUILD_GENERATOR=ON valid, then create the CLI target
# automatically once its entry point exists.
if(NOT EXISTS "${WORM_GENERATOR_ENTRYPOINT}")
  message(STATUS "Worm generator configured; CLI target awaits generator/src/main.cpp")
  return()
endif()

add_executable(WormGenerator "${WORM_GENERATOR_ENTRYPOINT}")
add_executable(Worm::Generator ALIAS WormGenerator)

set_target_properties(WormGenerator PROPERTIES OUTPUT_NAME worm-generator)

target_compile_features(WormGenerator PRIVATE cxx_std_20)
target_link_libraries(
  WormGenerator
  PRIVATE
    Worm::Connection
    Worm::Core
)
