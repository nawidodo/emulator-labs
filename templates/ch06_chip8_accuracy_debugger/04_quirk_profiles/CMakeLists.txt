add_executable(ch06_04_quirks_tests main.cpp)
target_link_libraries(ch06_04_quirks_tests PRIVATE labstest)
add_test(NAME ch06_04_quirks COMMAND ch06_04_quirks_tests)

add_executable(ch06_04_runner runner_main.cpp)
add_test(NAME ch06_04_runner_help COMMAND ch06_04_runner --help)
