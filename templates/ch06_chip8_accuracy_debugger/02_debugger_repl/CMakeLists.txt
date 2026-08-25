add_executable(ch06_02_repl_tests main.cpp)
target_link_libraries(ch06_02_repl_tests PRIVATE labstest)
add_test(NAME ch06_02_repl COMMAND ch06_02_repl_tests)

add_executable(ch06_02_repl repl_main.cpp)
add_test(NAME ch06_02_repl_help COMMAND ch06_02_repl --help)
