add_executable(ch37_04_opt_tests main.cpp)
target_link_libraries(ch37_04_opt_tests PRIVATE labstest)
add_test(NAME ch37_04_opt COMMAND ch37_04_opt_tests)

add_executable(ch37_04_runner opt_runner.cpp)
add_test(NAME ch37_04_runner_help COMMAND ch37_04_runner --help)
