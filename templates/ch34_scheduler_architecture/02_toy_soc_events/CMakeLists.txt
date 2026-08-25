add_executable(ch34_02_toy_soc_tests main.cpp)
target_link_libraries(ch34_02_toy_soc_tests PRIVATE labstest)
add_test(NAME ch34_02_toy_soc COMMAND ch34_02_toy_soc_tests)

add_executable(ch34_02_toy_soc_runner runner_main.cpp)
add_test(NAME ch34_02_toy_soc_runner_help
         COMMAND ch34_02_toy_soc_runner --help)
