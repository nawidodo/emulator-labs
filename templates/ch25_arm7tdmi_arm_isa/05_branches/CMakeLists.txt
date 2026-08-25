add_executable(ch25_05_branch_tests main.cpp)
target_link_libraries(ch25_05_branch_tests PRIVATE labstest)
add_test(NAME ch25_05_branch COMMAND ch25_05_branch_tests)

add_executable(ch25_runner runner_main.cpp)
add_test(NAME ch25_05_branch_runner_help COMMAND ch25_runner --help)
