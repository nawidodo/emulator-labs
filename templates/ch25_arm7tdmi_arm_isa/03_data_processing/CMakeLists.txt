add_executable(ch25_03_dp_tests main.cpp)
target_link_libraries(ch25_03_dp_tests PRIVATE labstest)
add_test(NAME ch25_03_dp COMMAND ch25_03_dp_tests)
