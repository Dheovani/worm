function(add_worm_error_test target source test_name)
  add_executable(${target} ${source})
  target_link_libraries(${target} PRIVATE Worm::Errors)
  target_compile_features(${target} PRIVATE cxx_std_20)

  add_test(NAME ${test_name} COMMAND ${target})
  set_tests_properties(${test_name} PROPERTIES LABELS "unit;errors")
endfunction()

add_worm_error_test(
  WormExceptionTests
  worm-exception-tests.cpp
  Worm.Errors.WormException
)

add_worm_error_test(
  WormDatabaseExceptionTests
  database-exception-tests.cpp
  Worm.Errors.DatabaseException
)

add_worm_error_test(
  WormInvalidArgExceptionTests
  invalid-arg-exception-tests.cpp
  Worm.Errors.InvalidArgException
)

add_worm_error_test(
  WormInvalidArgTypeExceptionTests
  invalid-arg-type-exception-tests.cpp
  Worm.Errors.InvalidArgTypeException
)

add_worm_error_test(
  WormConfigurationExceptionTests
  configuration-exception-tests.cpp
  Worm.Errors.ConfigurationException
)

add_worm_error_test(
  WormMissingConfigurationExceptionTests
  missing-configuration-exception-tests.cpp
  Worm.Errors.MissingConfigurationException
)

add_worm_error_test(
  WormUnsupportedDatabaseExceptionTests
  unsupported-database-exception-tests.cpp
  Worm.Errors.UnsupportedDatabaseException
)

add_worm_error_test(
  WormDependencyInjectionExceptionTests
  dependency-injection-exception-tests.cpp
  Worm.Errors.DependencyInjectionException
)

add_worm_error_test(
  WormDependencyResolutionExceptionTests
  dependency-resolution-exception-tests.cpp
  Worm.Errors.DependencyResolutionException
)

add_worm_error_test(
  WormUnregisteredDependencyExceptionTests
  unregistered-dependency-exception-tests.cpp
  Worm.Errors.UnregisteredDependencyException
)

add_worm_error_test(
  WormDatabaseConnectionExceptionTests
  database-connection-exception-tests.cpp
  Worm.Errors.DatabaseConnectionException
)

add_worm_error_test(
  WormQueryExecutionExceptionTests
  query-execution-exception-tests.cpp
  Worm.Errors.QueryExecutionException
)

add_worm_error_test(
  WormTransactionExceptionTests
  transaction-exception-tests.cpp
  Worm.Errors.TransactionException
)

add_worm_error_test(
  WormSqlBuildExceptionTests
  sql-build-exception-tests.cpp
  Worm.Errors.SqlBuildException
)

add_worm_error_test(
  WormReflectionExceptionTests
  reflection-exception-tests.cpp
  Worm.Errors.ReflectionException
)

add_worm_error_test(
  WormMappingExceptionTests
  mapping-exception-tests.cpp
  Worm.Errors.MappingException
)

add_worm_error_test(
  WormHydrationExceptionTests
  hydration-exception-tests.cpp
  Worm.Errors.HydrationException
)

add_worm_error_test(
  WormTypeConversionExceptionTests
  type-conversion-exception-tests.cpp
  Worm.Errors.TypeConversionException
)
