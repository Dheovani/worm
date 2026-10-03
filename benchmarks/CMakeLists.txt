add_executable(WormCoreBenchmarks core-benchmarks.cpp)
target_link_libraries(WormCoreBenchmarks PRIVATE Worm::Core)
target_compile_features(WormCoreBenchmarks PRIVATE cxx_std_20)

add_executable(WormDriverBenchmarks driver-benchmarks.cpp)
target_link_libraries(WormDriverBenchmarks PRIVATE Worm::Connection Worm::Core)
target_compile_features(WormDriverBenchmarks PRIVATE cxx_std_20)
