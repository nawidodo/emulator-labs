add_executable(ch10_02_meta_tests main.cpp)
target_link_libraries(ch10_02_meta_tests PRIVATE labstest)
add_test(NAME ch10_02_meta COMMAND ch10_02_meta_tests)
