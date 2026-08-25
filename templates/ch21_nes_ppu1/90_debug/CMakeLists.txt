add_executable(ch21_dbg_tests main.cpp)
target_link_libraries(ch21_dbg_tests PRIVATE labstest)
add_test(NAME ch21_90_debug COMMAND ch21_dbg_tests)
