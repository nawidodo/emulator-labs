add_executable(ch21_03_render_tests main.cpp)
target_link_libraries(ch21_03_render_tests PRIVATE labstest)
add_test(NAME ch21_03_render COMMAND ch21_03_render_tests)

add_executable(ch21_03_render_runner runner_main.cpp)
add_test(NAME ch21_03_render_runner_help
         COMMAND ch21_03_render_runner --help)
