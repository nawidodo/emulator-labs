add_executable(ch23_02_uxcn_tests main.cpp)
target_link_libraries(ch23_02_uxcn_tests PRIVATE labstest)
add_test(NAME ch23_02_uxcn COMMAND ch23_02_uxcn_tests)
