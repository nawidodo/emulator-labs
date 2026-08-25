add_executable(ch14_01_tile_tests main.cpp)
target_link_libraries(ch14_01_tile_tests PRIVATE labstest)
add_test(NAME ch14_01_tile COMMAND ch14_01_tile_tests)
