add_executable(ch06_01_trace_tests main.cpp)
target_link_libraries(ch06_01_trace_tests PRIVATE labstest)
add_test(NAME ch06_01_trace COMMAND ch06_01_trace_tests)

add_executable(ch06_01_trace_runner runner_main.cpp)
add_test(NAME ch06_01_trace_runner_help COMMAND ch06_01_trace_runner --help)

# Smoke: a tiny synthetic program traces deterministically (fixture is
# assembled on the fly so this ctest needs no repo-relative file paths).
add_test(NAME ch06_01_trace_runner_smoke
         COMMAND ch06_01_trace_runner --rom ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/prog_smoke.bin
                 --cycles 6 --trace ${CMAKE_CURRENT_BINARY_DIR}/smoke.log)
