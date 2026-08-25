add_executable(ch29_05_scheduler_tests main.cpp)
target_link_libraries(ch29_05_scheduler_tests PRIVATE labstest)
add_test(NAME ch29_05_scheduler COMMAND ch29_05_scheduler_tests)

add_executable(ch29_hw_runner runner_main.cpp)
target_link_libraries(ch29_hw_runner PRIVATE labstest)
add_test(NAME ch29_hw_runner_help COMMAND ch29_hw_runner --help)
