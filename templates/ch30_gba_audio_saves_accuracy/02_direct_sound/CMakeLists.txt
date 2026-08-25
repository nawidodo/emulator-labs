add_executable(ch30_02_dsound_tests main.cpp)
target_link_libraries(ch30_02_dsound_tests PRIVATE labstest)
add_test(NAME ch30_02_dsound COMMAND ch30_02_dsound_tests)

add_executable(ch30_audio_runner audio_runner.cpp)
target_link_libraries(ch30_audio_runner PRIVATE labstest)
add_test(NAME ch30_audio_runner_help COMMAND ch30_audio_runner --help)
