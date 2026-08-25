add_executable(ch02_02_alu_tests main.cpp)
target_link_libraries(ch02_02_alu_tests PRIVATE labstest)
add_test(NAME ch02_02_alu COMMAND ch02_02_alu_tests)
