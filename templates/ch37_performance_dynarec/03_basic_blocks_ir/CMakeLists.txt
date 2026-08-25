add_executable(ch37_03_ir_tests main.cpp)
target_link_libraries(ch37_03_ir_tests PRIVATE labstest)
add_test(NAME ch37_03_ir COMMAND ch37_03_ir_tests)

add_executable(ch37_03_runner ir_runner.cpp)
add_test(NAME ch37_03_runner_help COMMAND ch37_03_runner --help)
