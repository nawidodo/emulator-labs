add_executable(ch24_dbg_tests main.cpp)
target_link_libraries(ch24_dbg_tests PRIVATE labstest)
add_test(NAME ch24_dbg COMMAND ch24_dbg_tests)
