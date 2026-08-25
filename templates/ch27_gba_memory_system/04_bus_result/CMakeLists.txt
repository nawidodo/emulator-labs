add_executable(ch27_04_bus_tests main.cpp)
target_link_libraries(ch27_04_bus_tests PRIVATE labstest)
add_test(NAME ch27_04_bus COMMAND ch27_04_bus_tests)
