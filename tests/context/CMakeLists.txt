add_executable(WormPersistenceContextTests persistence-context-tests.cpp)

target_link_libraries(WormPersistenceContextTests PRIVATE Worm::Context)
target_compile_features(WormPersistenceContextTests PRIVATE cxx_std_20)

add_test(NAME Worm.Context.PersistenceContext COMMAND WormPersistenceContextTests)
set_tests_properties(
  Worm.Context.PersistenceContext
  PROPERTIES
    LABELS "integration;context"
    TIMEOUT 10
)
