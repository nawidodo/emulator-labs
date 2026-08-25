add_executable(ch31_03_exec_tests main.cpp)
target_link_libraries(ch31_03_exec_tests PRIVATE labstest)
target_compile_definitions(ch31_03_exec_tests PRIVATE
    LABS_SRC_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME ch31_03_exec COMMAND ch31_03_exec_tests)

add_executable(ch31_03_exec_runner runner_main.cpp)
target_link_libraries(ch31_03_exec_runner PRIVATE labstest)
add_test(NAME ch31_03_exec_runner_help COMMAND ch31_03_exec_runner --help)
