add_executable(i8080_diag_tests main.cpp cpu.cpp)
target_link_libraries(i8080_diag_tests PRIVATE labstest)
target_include_directories(i8080_diag_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME ch08_91_challenge COMMAND i8080_diag_tests)

add_executable(i8080_diag_runner runner_main.cpp cpu.cpp)
target_include_directories(i8080_diag_runner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME ch08_91_challenge_runner_help COMMAND i8080_diag_runner --help)
