add_executable(ch17_02_wave_tests main.cpp)
target_link_libraries(ch17_02_wave_tests PRIVATE labstest)
add_test(NAME ch17_02_wave COMMAND ch17_02_wave_tests)
