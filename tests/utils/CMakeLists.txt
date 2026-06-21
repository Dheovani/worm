function(add_worm_utils_test target source test_name)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE Worm::Utils)
    target_compile_features(${target} PRIVATE cxx_std_20)

    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(${test_name} PROPERTIES LABELS "unit;utils")
endfunction()

add_worm_utils_test(WormHelpersTests helpers-tests.cpp Worm.Utils.Helpers)
add_worm_utils_test(WormHashTests hash-tests.cpp Worm.Utils.Hash)
add_worm_utils_test(WormLoggerTests logger-tests.cpp Worm.Utils.Logger)
add_worm_utils_test(WormClientFactoryTests client-factory-tests.cpp Worm.Utils.ClientFactory)
add_worm_utils_test(
    WormDependencyInjectionTests
    dependency-injection-tests.cpp
    Worm.Utils.DependencyInjection
)

set_tests_properties(
    Worm.Utils.Helpers
    Worm.Utils.ClientFactory
    Worm.Utils.DependencyInjection
    PROPERTIES LABELS "integration;utils"
)
