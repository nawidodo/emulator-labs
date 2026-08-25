add_executable(ch12_04_boot_tests main.cpp)
target_link_libraries(ch12_04_boot_tests PRIVATE labstest)
add_test(NAME ch12_04_boot COMMAND ch12_04_boot_tests)
