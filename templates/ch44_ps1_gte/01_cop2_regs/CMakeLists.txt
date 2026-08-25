add_executable(ch44_01_cop2_tests main.cpp)
target_link_libraries(ch44_01_cop2_tests PRIVATE labstest)
add_test(NAME ch44_01_cop2 COMMAND ch44_01_cop2_tests)
