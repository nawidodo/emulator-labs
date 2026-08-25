add_executable(ch37_99_ext_tests ext_main.cpp)
target_link_libraries(ch37_99_ext_tests PRIVATE labstest)
add_test(NAME ch37_99_ext COMMAND ch37_99_ext_tests)

add_executable(ch37_99_ext_runner ext_runner.cpp)
add_test(NAME ch37_99_ext_runner_help COMMAND ch37_99_ext_runner --help)
