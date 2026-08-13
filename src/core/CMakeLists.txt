add_library(
  WormCore
  query/criteria.cpp
  query/dialect.cpp
  query/filter.cpp
  query/n-plus-one-detector.cpp
  query/predicate.cpp
  query/query-builder.cpp
  query/sql-builder.cpp
  query/statement.cpp
  query/validator.cpp
  output/paginator.cpp
  output/result-set.cpp
  persistence/session.cpp
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
    Worm::Errors
    Worm::Reflection
    Worm::Utils
  PRIVATE
    Worm::Connection
)
