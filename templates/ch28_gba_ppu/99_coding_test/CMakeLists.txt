add_executable(ch28_coding_tests main.cpp)
target_link_libraries(ch28_coding_tests PRIVATE labstest)
add_test(NAME ch28_99_coding COMMAND ch28_coding_tests)

add_executable(ch28_snapshot_tool snapshot_tool.cpp)
target_link_libraries(ch28_snapshot_tool PRIVATE labstest)
add_test(NAME ch28_snapshot_tool_help COMMAND ch28_snapshot_tool --help)
