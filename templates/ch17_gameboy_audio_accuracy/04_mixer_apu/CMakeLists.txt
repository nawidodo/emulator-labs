add_executable(ch17_04_apu_tests main.cpp)
target_link_libraries(ch17_04_apu_tests PRIVATE labstest)
add_test(NAME ch17_04_apu COMMAND ch17_04_apu_tests)

add_executable(ch17_04_apu_runner runner_main.cpp)
target_link_libraries(ch17_04_apu_runner PRIVATE labstest)
add_test(NAME ch17_04_apu_runner_help
         COMMAND ch17_04_apu_runner --help)
