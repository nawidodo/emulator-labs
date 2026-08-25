add_executable(ch04_05_random_key_tests main.cpp)
target_link_libraries(ch04_05_random_key_tests PRIVATE labstest)
add_test(NAME ch04_05_random_key COMMAND ch04_05_random_key_tests)

add_executable(ch04_05_random_key_runner runner_main.cpp)
add_test(NAME ch04_05_random_key_runner_help
         COMMAND ch04_05_random_key_runner --help)
