add_executable(ch25_challenge_tests main.cpp)
target_link_libraries(ch25_challenge_tests PRIVATE labstest)
add_test(NAME ch25_91_challenge COMMAND ch25_challenge_tests)

add_executable(ch25_challenge_runner runner_main.cpp)
add_test(NAME ch25_91_challenge_runner_help COMMAND ch25_challenge_runner --help)
