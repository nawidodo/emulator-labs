add_executable(ch42_90_debug_tests tests.cpp debug_stages.hpp)
target_link_libraries(ch42_90_debug_tests PRIVATE labstest)
add_test(NAME ch42_90_debug COMMAND ch42_90_debug_tests)

add_executable(ch42_90_debug_runner runner_main.cpp debug_stages.hpp)
add_test(NAME ch42_90_debug_runner_help COMMAND ch42_90_debug_runner --help)
