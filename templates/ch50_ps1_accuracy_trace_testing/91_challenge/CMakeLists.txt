add_executable(ch50_91_challenge_tests
    main.cpp)
target_link_libraries(ch50_91_challenge_tests PRIVATE labstest)

add_test(NAME ch50_91_challenge COMMAND ch50_91_challenge_tests)

set_tests_properties(ch50_91_challenge PROPERTIES LABELS accuracy)
