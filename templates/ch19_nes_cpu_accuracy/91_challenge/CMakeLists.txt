add_executable(ch19_91_challenge_tests main.cpp)
target_link_libraries(ch19_91_challenge_tests PRIVATE labstest)
add_test(NAME ch19_91_challenge COMMAND ch19_91_challenge_tests)

add_executable(ch19_91_challenge_runner runner_main.cpp)
add_test(NAME ch19_91_challenge_runner_help
         COMMAND ch19_91_challenge_runner --help)
