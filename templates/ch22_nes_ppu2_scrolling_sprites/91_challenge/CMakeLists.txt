add_executable(ch22_91_timing_tests main.cpp)
target_link_libraries(ch22_91_timing_tests PRIVATE labstest)
add_test(NAME ch22_91_timing COMMAND ch22_91_timing_tests)

add_executable(ch22_91_timing_runner timing_runner.cpp)
add_test(NAME ch22_91_timing_runner_help
         COMMAND ch22_91_timing_runner --help)
