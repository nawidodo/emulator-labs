add_executable(ch26_02_pipeline_tests main.cpp)
target_link_libraries(ch26_02_pipeline_tests PRIVATE labstest)
add_test(NAME ch26_02_pipeline COMMAND ch26_02_pipeline_tests)

add_executable(ch26_pipeline_runner runner_main.cpp)
add_test(NAME ch26_02_pipeline_runner_help COMMAND ch26_pipeline_runner --help)
