add_executable(i8080_stack_tests main.cpp)
target_link_libraries(i8080_stack_tests PRIVATE labstest)
target_include_directories(i8080_stack_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME ch08_01_stack COMMAND i8080_stack_tests)
