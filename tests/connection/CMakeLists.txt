function(add_worm_connection_test target source test_name)
  add_executable(${target} ${source})
  target_link_libraries(${target} PRIVATE Worm::Connection)
  target_compile_features(${target} PRIVATE cxx_std_20)

  add_test(NAME ${test_name} COMMAND ${target})
  set_tests_properties(${test_name} PROPERTIES LABELS "unit;connection")
endfunction()

function(add_worm_driver_contract_test target source test_name driver_label)
  add_executable(${target} ${source})
  target_link_libraries(${target} PRIVATE Worm::Connection)
  target_compile_features(${target} PRIVATE cxx_std_20)

  add_test(NAME ${test_name} COMMAND ${target})
  set_tests_properties(
    ${test_name}
    PROPERTIES
      LABELS "integration;connection;driver-contract;${driver_label}"
      SKIP_RETURN_CODE 77
      TIMEOUT 30
  )
endfunction()

add_worm_connection_test(
  WormClientTests
  client-tests.cpp
  Worm.Connection.Client
)

add_worm_connection_test(
  WormClientCacheTests
  client-cache-tests.cpp
  Worm.Connection.ClientCache
)

add_worm_connection_test(
  WormConfigurationTests
  configuration-tests.cpp
  Worm.Connection.Configuration
)

add_worm_connection_test(
  WormTransactionTests
  transaction-tests.cpp
  Worm.Connection.Transaction
)

set_tests_properties(Worm.Connection.Transaction PROPERTIES TIMEOUT 10)

if(WORM_ENABLE_MYSQL)
  add_worm_connection_test(
    WormMySqlClientTests
    drivers/mysql-client-tests.cpp
    Worm.Connection.MySqlClient
  )

  add_worm_driver_contract_test(
    WormMySqlDriverContractTests
    drivers/mysql-contract-tests.cpp
    Worm.Connection.Contract.MySQL
    mysql
  )
endif()

if(WORM_ENABLE_POSTGRESQL)
  add_worm_connection_test(
    WormPgClientTests
    drivers/pg-client-tests.cpp
    Worm.Connection.PgClient
  )
endif()

if(WORM_ENABLE_POSTGRESQL AND NOT WIN32)
  add_worm_driver_contract_test(
    WormPostgresDriverContractTests
    drivers/pg-contract-tests.cpp
    Worm.Connection.Contract.PostgreSQL
    postgresql
  )
endif()

if(WORM_ENABLE_SQLITE)
  add_worm_connection_test(
    WormSqliteClientTests
    drivers/sqlite-client-tests.cpp
    Worm.Connection.SqliteClient
  )

  set_tests_properties(
    Worm.Connection.SqliteClient
    PROPERTIES LABELS "integration;connection;sqlite"
  )

  add_worm_driver_contract_test(
    WormSqliteDriverContractTests
    drivers/sqlite-contract-tests.cpp
    Worm.Connection.Contract.SQLite
    sqlite
  )
endif()

if(WORM_ENABLE_MSSQL)
  add_worm_connection_test(
    WormMsSqlClientTests
    drivers/mssql-client-tests.cpp
    Worm.Connection.MsSqlClient
  )
endif()
