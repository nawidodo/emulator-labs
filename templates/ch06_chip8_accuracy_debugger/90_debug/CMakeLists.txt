add_executable(ch06_90_debug_tests main.cpp)
target_link_libraries(ch06_90_debug_tests PRIVATE labstest)
add_test(NAME ch06_90_debug COMMAND ch06_90_debug_tests)

add_executable(ch06_90_debug_runner runner_main.cpp)
add_test(NAME ch06_90_debug_runner_help COMMAND ch06_90_debug_runner --help)
