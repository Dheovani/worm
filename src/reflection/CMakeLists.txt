add_library(WormReflection INTERFACE)
add_library(Worm::Reflection ALIAS WormReflection)

set_target_properties(WormReflection PROPERTIES EXPORT_NAME Reflection)

target_compile_features(WormReflection INTERFACE cxx_std_20)
target_compile_options(
  WormReflection
  INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>
)

target_include_directories(
  WormReflection
  INTERFACE
    $<BUILD_INTERFACE:${PROJECT_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include>
)
