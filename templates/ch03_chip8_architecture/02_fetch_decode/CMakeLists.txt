add_executable(ch03_02_fetch_tests main.cpp)
target_link_libraries(ch03_02_fetch_tests PRIVATE labstest)
add_test(NAME ch03_02_fetch COMMAND ch03_02_fetch_tests)

add_executable(ch03_02_fetch_runner runner_main.cpp)
add_test(NAME ch03_02_fetch_runner_help COMMAND ch03_02_fetch_runner --help)
