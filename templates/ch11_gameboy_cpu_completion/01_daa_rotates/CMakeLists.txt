add_executable(ch11_01_daa_tests main.cpp)
target_link_libraries(ch11_01_daa_tests PRIVATE labstest)
add_test(NAME ch11_01_daa COMMAND ch11_01_daa_tests)
