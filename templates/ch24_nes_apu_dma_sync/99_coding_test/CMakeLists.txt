add_executable(ch24_99_drift_tests main.cpp)
target_link_libraries(ch24_99_drift_tests PRIVATE labstest)
add_test(NAME ch24_99_drift COMMAND ch24_99_drift_tests)

add_executable(ch24_99_drift_runner drift_runner.cpp)
add_test(NAME ch24_99_drift_runner_help COMMAND ch24_99_drift_runner --help)
