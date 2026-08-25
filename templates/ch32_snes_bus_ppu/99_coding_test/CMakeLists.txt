add_executable(ch32_99_coding_tests main.cpp)
target_link_libraries(ch32_99_coding_tests PRIVATE labstest)
add_test(NAME ch32_99_coding_tests COMMAND ch32_99_coding_tests)
add_test(NAME ch32_99_coding_tests_help
         COMMAND ch32_99_coding_tests --help)
