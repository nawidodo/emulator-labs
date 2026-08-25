add_executable(ch04_04_memory_bcd_tests main.cpp)
target_link_libraries(ch04_04_memory_bcd_tests PRIVATE labstest)
add_test(NAME ch04_04_memory_bcd COMMAND ch04_04_memory_bcd_tests)

add_executable(ch04_04_memory_bcd_runner runner_main.cpp)
add_test(NAME ch04_04_memory_bcd_runner_help
         COMMAND ch04_04_memory_bcd_runner --help)
