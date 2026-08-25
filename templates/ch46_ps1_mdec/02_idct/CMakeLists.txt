add_executable(ch46_02_idct_tests main.cpp)
target_link_libraries(ch46_02_idct_tests PRIVATE labstest)
add_test(NAME ch46_02_idct COMMAND ch46_02_idct_tests)
