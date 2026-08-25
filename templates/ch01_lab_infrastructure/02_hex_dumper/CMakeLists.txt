add_executable(ch01_02_hex_dumper dumper_main.cpp)
add_test(NAME ch01_02_hex_dumper_help COMMAND ch01_02_hex_dumper --help)

add_executable(ch01_02_hex_dumper_tests main.cpp)
target_link_libraries(ch01_02_hex_dumper_tests PRIVATE labstest)
add_test(NAME ch01_02_hex_dumper COMMAND ch01_02_hex_dumper_tests)
