add_executable(ch14_04_frame_tests main.cpp)
target_link_libraries(ch14_04_frame_tests PRIVATE labstest)
add_test(NAME ch14_04_frame COMMAND ch14_04_frame_tests)

add_executable(ch14_04_frame_runner runner_main.cpp)
add_test(NAME ch14_04_frame_runner_help
         COMMAND ch14_04_frame_runner --help)
