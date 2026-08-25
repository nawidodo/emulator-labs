add_executable(ch23_99_mfx_tests main.cpp)
target_link_libraries(ch23_99_mfx_tests PRIVATE labstest)
add_test(NAME ch23_99_mfx COMMAND ch23_99_mfx_tests)

add_executable(ch23_99_mfx_runner mfx_runner.cpp)
add_test(NAME ch23_99_mfx_runner_help COMMAND ch23_99_mfx_runner --help)
