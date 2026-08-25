add_executable(ch04_06_flags_suite_tests main.cpp)
target_link_libraries(ch04_06_flags_suite_tests PRIVATE labstest)
add_test(NAME ch04_06_flags_suite COMMAND ch04_06_flags_suite_tests)

add_executable(ch04_06_flags_suite_runner runner_main.cpp)
add_test(NAME ch04_06_flags_suite_runner_help
         COMMAND ch04_06_flags_suite_runner --help)
