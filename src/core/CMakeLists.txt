add_library(
  WormCore
  model/migration.cpp
  model/migration-history.cpp
  model/schema.cpp
  model/schema-diff.cpp
  model/schema-metadata.cpp
  model/schema-snapshot.cpp
  query/criteria.cpp
  query/dialect.cpp
  query/filter.cpp
  query/predicate.cpp
  query/query-builder.cpp
  query/sql-builder.cpp
  query/statement.cpp
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
