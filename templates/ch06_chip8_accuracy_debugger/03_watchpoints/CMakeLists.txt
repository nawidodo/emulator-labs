add_executable(ch06_03_watch_tests main.cpp)
target_link_libraries(ch06_03_watch_tests PRIVATE labstest)
add_test(NAME ch06_03_watch COMMAND ch06_03_watch_tests)

add_executable(ch06_03_repl repl_main.cpp)
add_test(NAME ch06_03_repl_help COMMAND ch06_03_repl --help)
