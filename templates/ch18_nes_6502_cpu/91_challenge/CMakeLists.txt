add_executable(ch18_91_challenge_tests main.cpp)
target_link_libraries(ch18_91_challenge_tests PRIVATE labstest)
add_test(NAME ch18_91_challenge COMMAND ch18_91_challenge_tests)

add_executable(ch18_91_challenge_runner runner_main.cpp)
add_test(NAME ch18_91_challenge_runner_help
         COMMAND ch18_91_challenge_runner --help)
