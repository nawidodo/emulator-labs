add_executable(ch41_91_challenge_runner runner_main.cpp)
target_link_libraries(ch41_91_challenge_runner PRIVATE labstest)
add_test(NAME ch41_91_challenge_runner_help
         COMMAND ch41_91_challenge_runner --help)
