add_executable(ch46_01_rlz_tests main.cpp)
target_link_libraries(ch46_01_rlz_tests PRIVATE labstest)
add_test(NAME ch46_01_rlz COMMAND ch46_01_rlz_tests)
