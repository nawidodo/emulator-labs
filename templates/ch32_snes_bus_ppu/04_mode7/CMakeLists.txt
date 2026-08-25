add_executable(ch32_04_mode7_tests main.cpp)
target_link_libraries(ch32_04_mode7_tests PRIVATE labstest)
add_test(NAME ch32_04_mode7_tests COMMAND ch32_04_mode7_tests)

add_executable(ch32_04_mode7_runner runner_main.cpp)
add_test(NAME ch32_04_mode7_runner_help COMMAND ch32_04_mode7_runner --help)
