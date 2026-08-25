add_executable(ch09_91_challenge_tests main.cpp cpu.cpp)
target_link_libraries(ch09_91_challenge_tests PRIVATE labstest)
target_include_directories(ch09_91_challenge_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME ch09_91_challenge COMMAND ch09_91_challenge_tests)

add_executable(ch09_91_si_runner runner_main.cpp cpu.cpp)
add_test(NAME ch09_91_si_runner_help COMMAND ch09_91_si_runner --help)
