add_executable(ch37_01_rx8_tests main.cpp)
target_link_libraries(ch37_01_rx8_tests PRIVATE labstest)
add_test(NAME ch37_01_rx8 COMMAND ch37_01_rx8_tests)

add_executable(ch37_01_runner bench_runner.cpp)
add_test(NAME ch37_01_runner_help COMMAND ch37_01_runner --help)
