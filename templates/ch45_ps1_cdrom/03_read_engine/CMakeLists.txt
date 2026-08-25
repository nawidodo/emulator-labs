add_executable(ch45_03_read_tests main.cpp)
target_link_libraries(ch45_03_read_tests PRIVATE labstest)
add_test(NAME ch45_03_read COMMAND ch45_03_read_tests)
