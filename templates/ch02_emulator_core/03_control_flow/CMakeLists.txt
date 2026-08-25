add_executable(ch02_03_flow_tests main.cpp)
target_link_libraries(ch02_03_flow_tests PRIVATE labstest)
add_test(NAME ch02_03_flow COMMAND ch02_03_flow_tests)
