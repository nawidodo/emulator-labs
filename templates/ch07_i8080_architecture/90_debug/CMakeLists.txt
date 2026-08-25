add_executable(ch07_90_debug_tests main.cpp cpu.cpp)
target_link_libraries(ch07_90_debug_tests PRIVATE labstest)
target_include_directories(ch07_90_debug_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME ch07_90_debug COMMAND ch07_90_debug_tests)
