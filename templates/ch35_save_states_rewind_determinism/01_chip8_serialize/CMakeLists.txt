add_executable(ch35_01_chip8_tests main.cpp)
target_link_libraries(ch35_01_chip8_tests PRIVATE labstest)
add_test(NAME ch35_01_chip8 COMMAND ch35_01_chip8_tests)

add_executable(ch35_01_chip8_runner runner_main.cpp)
add_test(NAME ch35_01_chip8_runner_help
         COMMAND ch35_01_chip8_runner --help)
