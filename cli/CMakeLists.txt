add_library(
  WormCliCore
  STATIC
    src/database/diff.cpp
    src/database/inspect.cpp
    src/database/n-plus-one.cpp
    src/generator/check.cpp
    src/generator/pull.cpp
    src/generator/push.cpp
    src/helpers/connection.cpp
    src/helpers/file.cpp
    src/helpers/manifest.cpp
    src/parser.cpp
    src/runner.cpp
    src/validator.cpp
)
add_library(Worm::CliCore ALIAS WormCliCore)

find_package(nlohmann_json CONFIG REQUIRED)

target_compile_features(WormCliCore PUBLIC cxx_std_20)
target_include_directories(
  WormCliCore
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(
  WormCliCore
  PUBLIC
    Worm::Connection
    Worm::Core
    Worm::Errors
  PRIVATE
    nlohmann_json::nlohmann_json
)

add_executable(WormCli src/main.cpp)
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
