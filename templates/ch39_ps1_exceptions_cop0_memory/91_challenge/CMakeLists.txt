add_executable(ch39_91_challenge_tests main.cpp boot_mini.hpp fnv.hpp)
target_link_libraries(ch39_91_challenge_tests PRIVATE labstest)
add_test(NAME ch39_91_challenge COMMAND ch39_91_challenge_tests)

add_executable(ch39_91_challenge_runner runner_main.cpp boot_mini.hpp fnv.hpp)
add_test(NAME ch39_91_challenge_runner_help
         COMMAND ch39_91_challenge_runner --help)
