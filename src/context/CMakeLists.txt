add_library(
  WormContext
  session.cpp
  session-factory.cpp
)
add_library(Worm::Context ALIAS WormContext)

target_compile_features(WormContext PUBLIC cxx_std_20)

target_include_directories(
  WormContext
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_INCLUDE_DIR}>
)

target_link_libraries(
  WormContext
  PUBLIC
    Worm::Core
)
