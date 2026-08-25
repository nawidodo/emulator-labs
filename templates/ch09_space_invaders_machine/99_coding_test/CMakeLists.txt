add_executable(ch09_99_si_b_tests main.cpp cpu.cpp)
target_link_libraries(ch09_99_si_b_tests PRIVATE labstest)
target_include_directories(ch09_99_si_b_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME ch09_99_si_b COMMAND ch09_99_si_b_tests)

add_executable(ch09_99_si_b_runner runner_main.cpp cpu.cpp)
add_test(NAME ch09_99_si_b_runner_help COMMAND ch09_99_si_b_runner --help)
