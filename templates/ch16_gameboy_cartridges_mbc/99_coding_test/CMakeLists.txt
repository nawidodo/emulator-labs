add_executable(ch16_99_mbcx_tests main.cpp unseen_mapper.cpp)
target_link_libraries(ch16_99_mbcx_tests PRIVATE labstest)
add_test(NAME ch16_99_mbcx COMMAND ch16_99_mbcx_tests)
