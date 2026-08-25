add_executable(ch01_99_coding_test checker.cpp)
target_compile_definitions(ch01_99_coding_test
                           PRIVATE EXERCISE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME ch01_99_coding_test COMMAND ch01_99_coding_test coding-test)
