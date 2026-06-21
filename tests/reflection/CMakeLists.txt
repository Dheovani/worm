function(add_worm_reflection_test target source test_name)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE Worm::Reflection)
    target_compile_features(${target} PRIVATE cxx_std_20)

    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(${test_name} PROPERTIES LABELS "unit;reflection")
endfunction()

add_worm_reflection_test(WormFieldTests field-tests.cpp Worm.Reflection.Field)
add_worm_reflection_test(WormConceptsTests concepts-tests.cpp Worm.Reflection.Concepts)
add_worm_reflection_test(WormVisitTests visit-tests.cpp Worm.Reflection.Visit)
