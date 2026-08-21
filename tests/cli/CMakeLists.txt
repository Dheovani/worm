add_executable(WormCliResolverTests resolver-tests.cpp)
target_link_libraries(WormCliResolverTests PRIVATE WormCliCore)
target_compile_features(WormCliResolverTests PRIVATE cxx_std_20)

add_test(NAME Worm.Cli.Resolver COMMAND WormCliResolverTests)
set_tests_properties(Worm.Cli.Resolver PROPERTIES LABELS "unit;cli")
