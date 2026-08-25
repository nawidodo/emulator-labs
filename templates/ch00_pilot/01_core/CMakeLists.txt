add_executable(pilot_tests main.cpp)
target_link_libraries(pilot_tests PRIVATE labstest)
add_test(NAME pilot COMMAND pilot_tests)
