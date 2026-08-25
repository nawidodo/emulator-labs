add_executable(i8080_flow_tests main.cpp cpu.cpp)
target_link_libraries(i8080_flow_tests PRIVATE labstest)
target_include_directories(i8080_flow_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME ch08_02_control_flow COMMAND i8080_flow_tests)

add_executable(i8080_flow_runner runner_main.cpp cpu.cpp)
target_include_directories(i8080_flow_runner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME ch08_02_control_flow_runner_help
         COMMAND i8080_flow_runner --help)
