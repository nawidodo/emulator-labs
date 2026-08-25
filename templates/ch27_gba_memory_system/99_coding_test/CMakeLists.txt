add_executable(ch27_99_coding_tests main.cpp)
target_link_libraries(ch27_99_coding_tests PRIVATE labstest)
add_test(NAME ch27_99_coding COMMAND ch27_99_coding_tests)

add_executable(ch27_flash_report calc_runner.cpp)
