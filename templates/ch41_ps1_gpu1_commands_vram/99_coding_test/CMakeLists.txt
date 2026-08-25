add_executable(ch41_99_coding_test_runner runner_main.cpp)
target_link_libraries(ch41_99_coding_test_runner PRIVATE labstest)
add_test(NAME ch41_99_coding_test_runner_help
         COMMAND ch41_99_coding_test_runner --help)
