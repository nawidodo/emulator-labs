cmake_minimum_required(VERSION 3.21)
# Strict-C17 seed lab: compiled with -std=c17 via the root language policy.
add_executable(c17_ch002_tests int_model.c int_model_tests.c)
target_include_directories(c17_ch002_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(c17_ch002_tests PRIVATE -pedantic)
add_test(NAME c17_ch002 COMMAND c17_ch002_tests)
