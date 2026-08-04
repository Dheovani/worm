add_library(WormUtils INTERFACE)
add_library(Worm::Utils ALIAS WormUtils)

target_compile_features(WormUtils INTERFACE cxx_std_20)
target_compile_options(
  WormUtils
  INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>
)

target_include_directories(
  WormUtils
  INTERFACE
    $<BUILD_INTERFACE:${PROJECT_INCLUDE_DIR}>
)
