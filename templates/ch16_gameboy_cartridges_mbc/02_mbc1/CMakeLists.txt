add_executable(ch16_02_mbc1_tests main.cpp)
target_link_libraries(ch16_02_mbc1_tests PRIVATE labstest)
add_test(NAME ch16_02_mbc1 COMMAND ch16_02_mbc1_tests)
