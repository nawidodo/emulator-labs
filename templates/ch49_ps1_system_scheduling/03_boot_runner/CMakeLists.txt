add_executable(ch49_03_boot_runner_tests main.cpp)
target_include_directories(ch49_03_boot_runner_tests PRIVATE ..)
target_link_libraries(ch49_03_boot_runner_tests PRIVATE labstest)
add_test(NAME ch49_03_boot_runner COMMAND ch49_03_boot_runner_tests)

add_executable(ch49_03_boot_runner_runner runner_main.cpp)
target_include_directories(ch49_03_boot_runner_runner PRIVATE ..)
add_test(NAME ch49_03_boot_runner_help
         COMMAND ch49_03_boot_runner_runner --help)
