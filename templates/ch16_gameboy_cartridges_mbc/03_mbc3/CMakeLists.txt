add_executable(ch16_03_mbc3_tests main.cpp)
target_link_libraries(ch16_03_mbc3_tests PRIVATE labstest)
add_test(NAME ch16_03_mbc3 COMMAND ch16_03_mbc3_tests)
