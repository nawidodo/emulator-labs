add_executable(ch25_coding_tests main.cpp)
target_link_libraries(ch25_coding_tests PRIVATE labstest)
add_test(NAME ch25_99_coding COMMAND ch25_coding_tests)

add_executable(ch25_coding_runner runner_main.cpp)
add_test(NAME ch25_99_coding_runner_help COMMAND ch25_coding_runner --help)
