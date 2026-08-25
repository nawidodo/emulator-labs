add_executable(ch11_91_smoke_tests smoke_main.cpp)
target_link_libraries(ch11_91_smoke_tests PRIVATE labstest)
add_test(NAME ch11_91_smoke COMMAND ch11_91_smoke_tests)

add_executable(ch11_91_cpu_runner runner_main.cpp)
add_test(NAME ch11_91_smoke_runner_help COMMAND ch11_91_cpu_runner --help)
