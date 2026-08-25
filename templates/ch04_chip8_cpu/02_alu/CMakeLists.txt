add_executable(ch04_02_alu_tests main.cpp)
target_link_libraries(ch04_02_alu_tests PRIVATE labstest)
add_test(NAME ch04_02_alu COMMAND ch04_02_alu_tests)

add_executable(ch04_02_alu_runner runner_main.cpp)
add_test(NAME ch04_02_alu_runner_help
         COMMAND ch04_02_alu_runner --help)
