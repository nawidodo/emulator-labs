add_executable(ch30_01_psg_tests main.cpp)
target_link_libraries(ch30_01_psg_tests PRIVATE labstest)
add_test(NAME ch30_01_psg COMMAND ch30_01_psg_tests)
