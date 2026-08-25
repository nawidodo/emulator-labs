add_executable(ch21_02_bg_tests main.cpp)
target_link_libraries(ch21_02_bg_tests PRIVATE labstest)
add_test(NAME ch21_02_bg COMMAND ch21_02_bg_tests)
