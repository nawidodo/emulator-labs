add_executable(ch02_01_fde_tests main.cpp)
target_link_libraries(ch02_01_fde_tests PRIVATE labstest)
add_test(NAME ch02_01_fde COMMAND ch02_01_fde_tests)
