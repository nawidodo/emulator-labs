add_executable(ch15_04_fullppu_tests main.cpp)
target_link_libraries(ch15_04_fullppu_tests PRIVATE labstest)
add_test(NAME ch15_04_fullppu COMMAND ch15_04_fullppu_tests)

add_executable(ch15_04_fullppu_runner runner_main.cpp)
add_test(NAME ch15_04_fullppu_runner_help
         COMMAND ch15_04_fullppu_runner --help)
