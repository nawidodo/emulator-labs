add_executable(ch24_03_fifo_tests main.cpp core.cpp)
target_link_libraries(ch24_03_fifo_tests PRIVATE labstest)
target_include_directories(ch24_03_fifo_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME ch24_03_fifo COMMAND ch24_03_fifo_tests)
