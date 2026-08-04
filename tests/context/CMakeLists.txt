add_executable(WormSessionTests session-tests.cpp)

target_link_libraries(WormSessionTests PRIVATE Worm::Context)
target_compile_features(WormSessionTests PRIVATE cxx_std_20)

add_test(NAME Worm.Context.Session COMMAND WormSessionTests)
set_tests_properties(
  Worm.Context.Session
  PROPERTIES
    LABELS "integration;context"
    TIMEOUT 10
)
