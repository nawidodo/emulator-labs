add_executable(ch44_02_rtps_tests main.cpp)
target_link_libraries(ch44_02_rtps_tests PRIVATE labstest)
add_test(NAME ch44_02_rtps COMMAND ch44_02_rtps_tests)
