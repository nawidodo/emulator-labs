add_executable(ch04_03_shift_quirks_tests main.cpp)
target_link_libraries(ch04_03_shift_quirks_tests PRIVATE labstest)
add_test(NAME ch04_03_shift_quirks COMMAND ch04_03_shift_quirks_tests)

add_executable(ch04_03_shift_quirks_runner runner_main.cpp)
add_test(NAME ch04_03_shift_quirks_runner_help
         COMMAND ch04_03_shift_quirks_runner --help)
