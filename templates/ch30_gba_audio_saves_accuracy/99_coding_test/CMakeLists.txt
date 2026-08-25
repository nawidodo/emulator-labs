add_executable(ch30_coding_tests main.cpp)
target_link_libraries(ch30_coding_tests PRIVATE labstest)
add_test(NAME ch30_99_coding COMMAND ch30_coding_tests)

add_executable(ch30_suite_runner suite_runner.cpp)
add_test(NAME ch30_suite_list COMMAND ch30_suite_runner --list-suites)
