add_executable(ch30_03_save_tests main.cpp)
target_link_libraries(ch30_03_save_tests PRIVATE labstest)
add_test(NAME ch30_03_save COMMAND ch30_03_save_tests)
