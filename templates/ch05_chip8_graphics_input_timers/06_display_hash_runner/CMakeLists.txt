add_executable(ch05_06_hash_tests main.cpp)
target_link_libraries(ch05_06_hash_tests PRIVATE labstest)
add_test(NAME ch05_06_hash COMMAND ch05_06_hash_tests)

add_executable(ch05_06_hash_runner runner_main.cpp)
add_test(NAME ch05_06_runner_help COMMAND ch05_06_hash_runner --help)
