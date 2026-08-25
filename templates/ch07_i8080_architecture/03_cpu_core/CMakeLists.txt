add_executable(i8080_core_tests main.cpp cpu.cpp)
target_link_libraries(i8080_core_tests PRIVATE labstest)
target_include_directories(i8080_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME ch07_03_cpu_core COMMAND i8080_core_tests)

add_executable(i8080_runner runner_main.cpp cpu.cpp)
target_include_directories(i8080_runner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME ch07_03_cpu_core_runner_help COMMAND i8080_runner --help)
