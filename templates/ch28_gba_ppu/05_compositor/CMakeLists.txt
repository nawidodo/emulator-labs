add_executable(ch28_05_compositor_tests main.cpp)
target_link_libraries(ch28_05_compositor_tests PRIVATE labstest)
add_test(NAME ch28_05_compositor COMMAND ch28_05_compositor_tests)

add_executable(ch28_ppu_runner runner_main.cpp)
target_link_libraries(ch28_ppu_runner PRIVATE labstest)
add_test(NAME ch28_ppu_runner_help COMMAND ch28_ppu_runner --help)
