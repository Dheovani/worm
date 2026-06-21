function(add_worm_connection_test target source test_name)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE Worm::Connection)
    target_compile_features(${target} PRIVATE cxx_std_20)

    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(${test_name} PROPERTIES LABELS "unit;connection")
endfunction()

add_worm_connection_test(
    WormClientTests
    client-tests.cpp
    Worm.Connection.Client
)

add_worm_connection_test(
    WormMySqlClientTests
    mysql-client-tests.cpp
    Worm.Connection.MySqlClient
)

add_worm_connection_test(
    WormPgClientTests
    pg-client-tests.cpp
    Worm.Connection.PgClient
)

add_worm_connection_test(
    WormSqliteClientTests
    sqlite-client-tests.cpp
    Worm.Connection.SqliteClient
)

set_tests_properties(
    Worm.Connection.SqliteClient
    PROPERTIES LABELS "integration;connection;sqlite"
)
