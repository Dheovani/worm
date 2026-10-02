add_library(WormUtils logger.cpp)
add_library(Worm::Utils ALIAS WormUtils)

target_compile_features(WormUtils PUBLIC cxx_std_20)
target_compile_options(
  WormUtils
  PUBLIC
    $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>
)

target_include_directories(
  WormUtils
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_INCLUDE_DIR}>
)
