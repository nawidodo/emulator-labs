add_executable(ch42_91_challenge_tests tests.cpp challenge_gpu.hpp fixture_quad.hpp)
target_link_libraries(ch42_91_challenge_tests PRIVATE labstest)
add_test(NAME ch42_91_challenge COMMAND ch42_91_challenge_tests)

add_executable(ch42_91_challenge_runner runner_main.cpp challenge_gpu.hpp)
add_test(NAME ch42_91_challenge_runner_help
         COMMAND ch42_91_challenge_runner --help)
