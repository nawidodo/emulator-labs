add_executable(ch32_03_render_tests main.cpp)
target_link_libraries(ch32_03_render_tests PRIVATE labstest)
add_test(NAME ch32_03_render_tests COMMAND ch32_03_render_tests)

add_executable(ch32_03_render_runner runner_main.cpp)
add_test(NAME ch32_03_render_runner_help COMMAND ch32_03_render_runner --help)
