add_executable(ch50_01_accuracy_runner
    runner_main.cpp)
target_link_libraries(ch50_01_accuracy_runner PRIVATE labstest)

add_executable(ch50_01_runner_tests
    main.cpp)
target_link_libraries(ch50_01_runner_tests PRIVATE labstest)

add_test(NAME ch50_01_runner_builtins COMMAND ch50_01_accuracy_runner)
add_test(NAME ch50_01_runner_help COMMAND ch50_01_accuracy_runner --list)
add_test(NAME ch50_01_runner_tests COMMAND ch50_01_runner_tests)

set_tests_properties(ch50_01_runner_builtins ch50_01_runner_help
    ch50_01_runner_tests PROPERTIES LABELS accuracy)
