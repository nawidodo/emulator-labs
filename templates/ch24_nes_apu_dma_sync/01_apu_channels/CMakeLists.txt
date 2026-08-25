add_executable(ch24_01_apu_tests main.cpp)
target_link_libraries(ch24_01_apu_tests PRIVATE labstest)
add_test(NAME ch24_01_apu COMMAND ch24_01_apu_tests)
