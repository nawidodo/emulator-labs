add_executable(c_mini_bits_tests main.cpp)
target_link_libraries(c_mini_bits_tests PRIVATE labstest)
add_test(NAME c_mini_bits COMMAND c_mini_bits_tests)
