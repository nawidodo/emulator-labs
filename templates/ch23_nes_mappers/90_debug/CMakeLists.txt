add_executable(ch23_dbg_tests main.cpp)
target_link_libraries(ch23_dbg_tests PRIVATE labstest)
add_test(NAME ch23_dbg COMMAND ch23_dbg_tests)
