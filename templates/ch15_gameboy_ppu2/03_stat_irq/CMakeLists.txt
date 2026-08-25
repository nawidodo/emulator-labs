add_executable(ch15_03_stat_tests main.cpp)
target_link_libraries(ch15_03_stat_tests PRIVATE labstest)
add_test(NAME ch15_03_stat COMMAND ch15_03_stat_tests)
