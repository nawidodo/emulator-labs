add_executable(ch48_90_debug_tests main.cpp)
target_include_directories(ch48_90_debug_tests PRIVATE ..)
target_link_libraries(ch48_90_debug_tests PRIVATE labstest)
add_test(NAME ch48_90_debug COMMAND ch48_90_debug_tests)
