add_executable(ch38_01_alu_tests main.cpp)
target_link_libraries(ch38_01_alu_tests PRIVATE labstest)
add_test(NAME ch38_01_alu COMMAND ch38_01_alu_tests)
