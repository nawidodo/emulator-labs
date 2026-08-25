add_executable(ch49_90_debug_tests main.cpp)
target_include_directories(ch49_90_debug_tests PRIVATE ..)
target_link_libraries(ch49_90_debug_tests PRIVATE labstest)
add_test(NAME ch49_90_debug COMMAND ch49_90_debug_tests)
