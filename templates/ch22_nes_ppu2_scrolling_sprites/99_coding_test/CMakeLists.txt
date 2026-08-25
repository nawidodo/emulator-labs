add_executable(ch22_99_frame_diff_tests main.cpp)
target_link_libraries(ch22_99_frame_diff_tests PRIVATE labstest)
add_test(NAME ch22_99_frame_diff COMMAND ch22_99_frame_diff_tests)

add_executable(ch22_99_frame_diff diff_main.cpp)
add_test(NAME ch22_99_frame_diff_help
         COMMAND ch22_99_frame_diff --help)
