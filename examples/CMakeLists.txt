if(NOT WORM_ENABLE_SQLITE)
  message(FATAL_ERROR "WORM_BUILD_EXAMPLES requires WORM_ENABLE_SQLITE=ON.")
endif()

function(worm_add_example target alias source)
  add_executable(${target} ${source})
  add_executable(Worm::${alias} ALIAS ${target})

  target_compile_features(${target} PRIVATE cxx_std_20)
  target_link_libraries(
    ${target}
    PRIVATE
      Worm::Connection
      Worm::Core
  )

  if(BUILD_TESTING)
    add_test(NAME Worm.Examples.${alias} COMMAND ${target})
    set_tests_properties(Worm.Examples.${alias} PROPERTIES LABELS "examples;sqlite")
  endif()
endfunction()

worm_add_example(WormSqliteQuickStart SqliteQuickStart sqlite-quick-start.cpp)
worm_add_example(WormSqliteQueries SqliteQueries sqlite-queries.cpp)
worm_add_example(WormSqliteRelationships SqliteRelationships sqlite-relationships.cpp)
worm_add_example(WormReflectionAndSnapshots ReflectionAndSnapshots reflection-and-snapshots.cpp)
worm_add_example(WormResultHydration ResultHydration result-hydration.cpp)
worm_add_example(WormSqliteSession SqliteSession sqlite-session.cpp)
worm_add_example(WormSqliteTransactions SqliteTransactions sqlite-transactions.cpp)
worm_add_example(WormSchemaMigrationPlan SchemaMigrationPlan schema-migration-plan.cpp)
worm_add_example(WormDialectStatements DialectStatements dialect-statements.cpp)
worm_add_example(WormSqliteView SqliteView sqlite-view.cpp)
