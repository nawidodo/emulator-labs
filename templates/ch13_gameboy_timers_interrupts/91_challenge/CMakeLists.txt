# Challenge: labstest suite over the embedded probe program + the headless
# runner binary used by the hidden grader.
add_executable(ch13_91_timer_tests main.cpp)
target_link_libraries(ch13_91_timer_tests PRIVATE labstest)
add_test(NAME ch13_91_timer COMMAND ch13_91_timer_tests)

add_executable(ch13_91_timer_runner runner_main.cpp)
add_test(NAME ch13_91_timer_runner_help COMMAND ch13_91_timer_runner --help)
