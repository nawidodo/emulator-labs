add_executable(ch22_dbg_tests main.cpp)
target_link_libraries(ch22_dbg_tests PRIVATE labstest)
add_test(NAME ch22_90_debug COMMAND ch22_dbg_tests)
