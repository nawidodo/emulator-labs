add_executable(ch41_01_vram_tests main.cpp)
target_link_libraries(ch41_01_vram_tests PRIVATE labstest)
add_test(NAME ch41_01_vram COMMAND ch41_01_vram_tests)
