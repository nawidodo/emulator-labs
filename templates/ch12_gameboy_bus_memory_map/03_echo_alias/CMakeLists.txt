add_executable(ch12_03_echo_tests main.cpp)
target_link_libraries(ch12_03_echo_tests PRIVATE labstest)
add_test(NAME ch12_03_echo COMMAND ch12_03_echo_tests)
