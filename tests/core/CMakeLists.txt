function(add_worm_core_test target source test_name)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE Worm::Core)
    target_compile_features(${target} PRIVATE cxx_std_20)

    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(${test_name} PROPERTIES LABELS "unit;core")
endfunction()

add_worm_core_test(WormParameterTests parameter-tests.cpp Worm.Core.Parameter)
add_worm_core_test(WormExpressionTests expression-tests.cpp Worm.Core.Expression)
add_worm_core_test(WormOrderingTests ordering-tests.cpp Worm.Core.Ordering)
add_worm_core_test(WormWhereClauseTests where-clause-tests.cpp Worm.Core.WhereClause)
