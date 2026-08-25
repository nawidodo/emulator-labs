add_executable(ch04_01_control_flow_tests main.cpp)
target_link_libraries(ch04_01_control_flow_tests PRIVATE labstest)
add_test(NAME ch04_01_control_flow COMMAND ch04_01_control_flow_tests)

add_executable(ch04_01_control_flow_runner runner_main.cpp)
add_test(NAME ch04_01_control_flow_runner_help
         COMMAND ch04_01_control_flow_runner --help)
