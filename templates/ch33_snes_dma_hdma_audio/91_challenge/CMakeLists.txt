add_executable(ch33_91_challenge_tests challenge_tests.cpp)
target_link_libraries(ch33_91_challenge_tests PRIVATE labstest)
add_test(NAME ch33_91_challenge_tests COMMAND ch33_91_challenge_tests)

add_executable(ch33_91_challenge_runner runner_main.cpp)
add_test(NAME ch33_91_challenge_runner_help
         COMMAND ch33_91_challenge_runner --help)
