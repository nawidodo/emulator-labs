add_executable(ch46_91_mdec_tests main.cpp)
target_link_libraries(ch46_91_mdec_tests PRIVATE labstest)
add_test(NAME ch46_91_mdec COMMAND ch46_91_mdec_tests)

add_executable(ch46_mdec_cli mdec_cli.cpp)
add_test(NAME ch46_mdec_cli_help COMMAND ch46_mdec_cli)
