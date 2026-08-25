add_executable(ch03_03_ops_tests main.cpp)
target_link_libraries(ch03_03_ops_tests PRIVATE labstest)
add_test(NAME ch03_03_ops COMMAND ch03_03_ops_tests)
