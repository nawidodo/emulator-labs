add_executable(ch23_03_mmc1_tests main.cpp)
target_link_libraries(ch23_03_mmc1_tests PRIVATE labstest)
add_test(NAME ch23_03_mmc1 COMMAND ch23_03_mmc1_tests)
