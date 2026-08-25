add_executable(ch20_91_boot_tests main.cpp)
target_link_libraries(ch20_91_boot_tests PRIVATE labstest)
add_test(NAME ch20_91_boot COMMAND ch20_91_boot_tests)

add_executable(ch20_91_boot_runner runner_main.cpp)
add_test(NAME ch20_91_boot_runner_help COMMAND ch20_91_boot_runner --help)
