add_executable(ch24_02_sched_tests main.cpp)
target_link_libraries(ch24_02_sched_tests PRIVATE labstest)
add_test(NAME ch24_02_sched COMMAND ch24_02_sched_tests)

add_executable(ch24_04_sync_runner runner_main.cpp)
add_test(NAME ch24_04_sync_runner_help COMMAND ch24_04_sync_runner --help)
