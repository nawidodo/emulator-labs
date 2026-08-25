add_executable(ch48_03_sio_tests main.cpp)
target_include_directories(ch48_03_sio_tests PRIVATE ..)
target_link_libraries(ch48_03_sio_tests PRIVATE labstest)
add_test(NAME ch48_03_sio COMMAND ch48_03_sio_tests)

add_executable(ch48_03_sio_runner runner_main.cpp)
target_link_libraries(ch48_03_sio_runner PRIVATE labstest)
add_test(NAME ch48_03_sio_runner_help COMMAND ch48_03_sio_runner --help)
