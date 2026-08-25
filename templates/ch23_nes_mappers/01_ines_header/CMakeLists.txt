add_executable(ch23_01_ines_tests main.cpp)
target_link_libraries(ch23_01_ines_tests PRIVATE labstest)
add_test(NAME ch23_01_ines COMMAND ch23_01_ines_tests)

add_executable(ch23_01_ines_runner runner_main.cpp)
add_test(NAME ch23_01_ines_runner_help COMMAND ch23_01_ines_runner --help)
