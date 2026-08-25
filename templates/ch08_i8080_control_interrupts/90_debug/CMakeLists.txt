add_executable(i8080_debug_tests main.cpp cpu.cpp)
target_link_libraries(i8080_debug_tests PRIVATE labstest)
target_include_directories(i8080_debug_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME ch08_90_debug COMMAND i8080_debug_tests)
