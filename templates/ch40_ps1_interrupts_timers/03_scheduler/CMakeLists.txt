add_executable(ch40_03_scheduler_tests main.cpp)
target_link_libraries(ch40_03_scheduler_tests PRIVATE labstest)
add_test(NAME ch40_03_scheduler COMMAND ch40_03_scheduler_tests)

add_executable(ch40_03_scheduler_runner runner_main.cpp)
add_test(NAME ch40_03_scheduler_runner_help
         COMMAND ch40_03_scheduler_runner --help)
