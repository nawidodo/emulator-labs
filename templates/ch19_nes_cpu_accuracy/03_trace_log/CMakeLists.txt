add_executable(ch19_03_trace_log_tests main.cpp)
target_link_libraries(ch19_03_trace_log_tests PRIVATE labstest)
add_test(NAME ch19_03_trace_log COMMAND ch19_03_trace_log_tests)

add_executable(ch19_03_trace_log_runner runner_main.cpp)
add_test(NAME ch19_03_trace_log_runner_help
         COMMAND ch19_03_trace_log_runner --help)
