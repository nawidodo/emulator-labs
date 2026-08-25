add_executable(ch45_01_disc_tests main.cpp)
target_link_libraries(ch45_01_disc_tests PRIVATE labstest)
add_test(NAME ch45_01_disc COMMAND ch45_01_disc_tests)
