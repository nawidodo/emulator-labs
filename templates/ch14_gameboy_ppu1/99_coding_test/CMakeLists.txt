add_executable(ch14_99_coding_tests main.cpp)
target_link_libraries(ch14_99_coding_tests PRIVATE labstest)
target_include_directories(ch14_99_coding_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME ch14_99_coding COMMAND ch14_99_coding_tests)

add_executable(ch14_99_coding_runner runner_main.cpp)
add_test(NAME ch14_99_coding_runner_help
         COMMAND ch14_99_coding_runner --help)
