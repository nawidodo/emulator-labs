add_executable(ch50_90_regress_tests
    main.cpp)
target_link_libraries(ch50_90_regress_tests PRIVATE labstest)

add_test(NAME ch50_90_regress COMMAND ch50_90_regress_tests)

set_tests_properties(ch50_90_regress PROPERTIES LABELS accuracy)
