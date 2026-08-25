add_executable(ch03_99_coding_tests main.cpp)
target_link_libraries(ch03_99_coding_tests PRIVATE labstest)
add_test(NAME ch03_99_coding COMMAND ch03_99_coding_tests)

add_executable(ch03_99_coding_runner runner_main.cpp)
add_test(NAME ch03_99_coding_runner_help COMMAND ch03_99_coding_runner --help)
