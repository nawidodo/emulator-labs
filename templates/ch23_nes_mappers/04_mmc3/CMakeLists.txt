add_executable(ch23_04_mmc3_tests main.cpp)
target_link_libraries(ch23_04_mmc3_tests PRIVATE labstest)
add_test(NAME ch23_04_mmc3 COMMAND ch23_04_mmc3_tests)
