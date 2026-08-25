add_executable(ch44_91_conf_tests main.cpp)
target_link_libraries(ch44_91_conf_tests PRIVATE labstest)
add_test(NAME ch44_91_conf COMMAND ch44_91_conf_tests)

add_executable(ch44_conf_runner conf_runner.cpp)
add_test(NAME ch44_conf_runner_help COMMAND ch44_conf_runner)
