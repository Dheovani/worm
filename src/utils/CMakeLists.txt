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

target_link_libraries(
  WormUtils
  PUBLIC
    Worm::Errors
)
