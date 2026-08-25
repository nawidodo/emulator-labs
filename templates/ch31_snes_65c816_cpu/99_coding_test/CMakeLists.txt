add_executable(ch31_coding_tests main.cpp)
target_link_libraries(ch31_coding_tests PRIVATE labstest)
add_test(NAME ch31_coding COMMAND ch31_coding_tests)
