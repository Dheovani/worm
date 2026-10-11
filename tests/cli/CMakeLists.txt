add_executable(WormCliResolverTests resolver-tests.cpp)
target_link_libraries(WormCliResolverTests PRIVATE WormCliCore)
target_compile_features(WormCliResolverTests PRIVATE cxx_std_20)

add_test(NAME Worm.Cli.Resolver COMMAND WormCliResolverTests)
set_tests_properties(Worm.Cli.Resolver PROPERTIES LABELS "unit;cli")

function(add_worm_cli_test target source test_name)
  add_executable(${target} ${source})
  target_link_libraries(${target} PRIVATE WormCliCore)
  target_compile_features(${target} PRIVATE cxx_std_20)
  add_test(NAME ${test_name} COMMAND ${target})
  set_tests_properties(${test_name} PROPERTIES LABELS "unit;cli")
endfunction()

add_worm_cli_test(WormCliManifestTests manifest-tests.cpp Worm.Cli.Manifest)
add_worm_cli_test(WormCliMigrationCatalogTests migration-catalog-tests.cpp Worm.Cli.MigrationCatalog)
add_worm_cli_test(WormCliMigrationFileTests migration-file-tests.cpp Worm.Cli.MigrationFile)
add_worm_cli_test(WormCliMigrationProgressTests migration-progress-tests.cpp Worm.Cli.MigrationProgress)
add_worm_cli_test(WormCliMigrateTests migrate-tests.cpp Worm.Cli.Migrate)
add_worm_cli_test(WormCliEntityManifestTests entity-manifest-tests.cpp Worm.Cli.EntityManifest)
add_worm_cli_test(WormCliFileTests file-tests.cpp Worm.Cli.File)
add_worm_cli_test(WormCliCheckTests check-tests.cpp Worm.Cli.Check)
add_worm_cli_test(WormCliDiffTests diff-tests.cpp Worm.Cli.Diff)
add_worm_cli_test(WormCliDoctorTests doctor-tests.cpp Worm.Cli.Doctor)
add_worm_cli_test(WormCliSeedTests seed-tests.cpp Worm.Cli.Seed)
add_worm_cli_test(WormCliPullTests pull-tests.cpp Worm.Cli.Pull)
add_worm_cli_test(WormCliPushTests push-tests.cpp Worm.Cli.Push)
add_worm_cli_test(WormCliInspectTests inspect-tests.cpp Worm.Cli.Inspect)
add_worm_cli_test(WormCliRunnerTests runner-tests.cpp Worm.Cli.Runner)
add_worm_cli_test(WormCliNPlusOneTests n-plus-one-tests.cpp Worm.Cli.NPlusOne)

if(WORM_ENABLE_SQLITE)
  add_worm_cli_test(WormCliSqliteCheckTests check-sqlite-tests.cpp Worm.Cli.Check.SQLite)
  add_worm_cli_test(WormCliSqliteDiffTests diff-sqlite-tests.cpp Worm.Cli.Diff.SQLite)
  add_worm_cli_test(WormCliSqliteMigrateTests migrate-sqlite-tests.cpp Worm.Cli.Migrate.SQLite)
  add_worm_cli_test(WormCliSqlitePushTests push-sqlite-tests.cpp Worm.Cli.Push.SQLite)
  add_worm_cli_test(WormCliSqliteSeedTests seed-sqlite-tests.cpp Worm.Cli.Seed.SQLite)
  set_tests_properties(Worm.Cli.Check.SQLite PROPERTIES LABELS "integration;cli;sqlite")
  set_tests_properties(Worm.Cli.Diff.SQLite PROPERTIES LABELS "integration;cli;sqlite")
  set_tests_properties(Worm.Cli.Migrate.SQLite PROPERTIES LABELS "integration;cli;sqlite")
  set_tests_properties(Worm.Cli.Push.SQLite PROPERTIES LABELS "integration;cli;sqlite")
  set_tests_properties(Worm.Cli.Seed.SQLite PROPERTIES LABELS "integration;cli;sqlite")
endif()

if(WORM_ENABLE_MSSQL)
  add_worm_cli_test(WormCliMsSqlContractTests mssql-cli-contract-tests.cpp Worm.Cli.MSSQL.Contract)
  set_tests_properties(
    Worm.Cli.MSSQL.Contract
    PROPERTIES
      LABELS "integration;cli;driver-contract;mssql"
      TIMEOUT 60
      SKIP_RETURN_CODE 77
      RESOURCE_LOCK mssql_database
  )
endif()
