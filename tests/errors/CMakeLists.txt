function(add_worm_error_test target source test_name)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE Worm::Errors)
    target_compile_features(${target} PRIVATE cxx_std_20)

    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(${test_name} PROPERTIES LABELS "unit;errors")
endfunction()

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
