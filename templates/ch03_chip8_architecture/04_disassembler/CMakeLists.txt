add_executable(ch03_04_disasm_tests main.cpp)
target_link_libraries(ch03_04_disasm_tests PRIVATE labstest)
add_test(NAME ch03_04_disasm COMMAND ch03_04_disasm_tests)

add_executable(ch03_04_disasm_runner runner_main.cpp)
add_test(NAME ch03_04_disasm_runner_help COMMAND ch03_04_disasm_runner --help)
