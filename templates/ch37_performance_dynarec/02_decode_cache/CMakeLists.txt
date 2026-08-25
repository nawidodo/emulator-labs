add_executable(ch37_02_cache_tests main.cpp)
target_link_libraries(ch37_02_cache_tests PRIVATE labstest)
add_test(NAME ch37_02_cache COMMAND ch37_02_cache_tests)

add_executable(ch37_02_runner cache_runner.cpp)
add_test(NAME ch37_02_runner_help COMMAND ch37_02_runner --help)
