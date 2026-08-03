add_library(WormContext INTERFACE)
add_library(Worm::Context ALIAS WormContext)

target_compile_features(WormContext INTERFACE cxx_std_20)

target_include_directories(
  WormContext
  INTERFACE
    $<BUILD_INTERFACE:${PROJECT_INCLUDE_DIR}>
)

target_link_libraries(
  WormContext
  INTERFACE
    Worm::Core
)
