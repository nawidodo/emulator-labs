add_executable(ch26_03_switch_tests main.cpp)
target_link_libraries(ch26_03_switch_tests PRIVATE labstest)
add_test(NAME ch26_03_switch COMMAND ch26_03_switch_tests)

add_executable(ch26_switch_runner runner_main.cpp)
add_test(NAME ch26_03_switch_runner_help COMMAND ch26_switch_runner --help)
