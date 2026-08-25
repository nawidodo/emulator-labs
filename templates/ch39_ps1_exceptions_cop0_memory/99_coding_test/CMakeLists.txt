add_executable(ch39_99_coding_test_tests main.cpp coding_test.hpp scenario.hpp)
target_link_libraries(ch39_99_coding_test_tests PRIVATE labstest)
add_test(NAME ch39_99_coding_test COMMAND ch39_99_coding_test_tests)

add_executable(ch39_99_coding_test_runner
               runner_main.cpp coding_test.hpp scenario.hpp
               ../91_challenge/fnv.hpp)
add_test(NAME ch39_99_coding_test_runner_help
         COMMAND ch39_99_coding_test_runner --help)
