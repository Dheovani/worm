add_library(
  WormUtils
  helpers.cpp
)
add_library(Worm::Utils ALIAS WormUtils)

target_compile_features(WormUtils PUBLIC cxx_std_20)

target_include_directories(
  WormUtils
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_INCLUDE_DIR}>
)

# Public utils headers expose JsonCpp and error types. Consumers that instantiate
# DependencyInjector<connection::Client> must also link Worm::Connection.
target_link_libraries(
  WormUtils
  PUBLIC
    ${WORM_JSONCPP_TARGET}
    Worm::Errors
)
