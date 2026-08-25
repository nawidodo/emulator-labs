add_executable(ch09_05_video_tests main.cpp cpu.cpp)
target_link_libraries(ch09_05_video_tests PRIVATE labstest)
target_include_directories(ch09_05_video_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME ch09_05_video COMMAND ch09_05_video_tests)

add_executable(ch09_05_si_runner runner_main.cpp cpu.cpp)
add_test(NAME ch09_05_si_runner_help COMMAND ch09_05_si_runner --help)
