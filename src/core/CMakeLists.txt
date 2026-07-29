add_library(
  WormCore
  query/dialect.cpp
  query/filter.cpp
  query/predicate.cpp
  query/sql-builder.cpp
  output/result-set.cpp
 "query/validator.cpp"
)
add_library(Worm::Core ALIAS WormCore)

target_compile_features(WormCore PUBLIC cxx_std_20)

target_include_directories(
  WormCore
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_INCLUDE_DIR}>
)

target_link_libraries(
  WormCore
  PUBLIC
    Worm::Connection
    Worm::Errors
    Worm::Reflection
    Worm::Utils
)
