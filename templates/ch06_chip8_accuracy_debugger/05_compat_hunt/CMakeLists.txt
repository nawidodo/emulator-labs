add_executable(ch06_05_hunt_tests main.cpp)
target_link_libraries(ch06_05_hunt_tests PRIVATE labstest)
add_test(NAME ch06_05_hunt COMMAND ch06_05_hunt_tests)

add_executable(ch06_05_runner runner_main.cpp)
add_test(NAME ch06_05_runner_help COMMAND ch06_05_runner --help)
