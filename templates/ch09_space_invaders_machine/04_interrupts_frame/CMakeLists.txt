add_executable(ch09_04_interrupts_tests main.cpp cpu.cpp)
target_link_libraries(ch09_04_interrupts_tests PRIVATE labstest)
target_include_directories(ch09_04_interrupts_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME ch09_04_interrupts COMMAND ch09_04_interrupts_tests)

add_executable(ch09_04_cadence_runner runner_main.cpp cpu.cpp)
add_test(NAME ch09_04_cadence_runner_help COMMAND ch09_04_cadence_runner --help)
