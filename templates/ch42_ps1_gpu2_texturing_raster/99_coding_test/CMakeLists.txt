add_executable(ch42_99_coding_test_runner runner_main.cpp coding_stages.hpp)
add_test(NAME ch42_99_coding_test_runner_help
         COMMAND ch42_99_coding_test_runner --help)
