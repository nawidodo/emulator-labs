add_executable(ch05_02_dxyn_tests main.cpp)
target_link_libraries(ch05_02_dxyn_tests PRIVATE labstest)
add_test(NAME ch05_02_dxyn COMMAND ch05_02_dxyn_tests)
