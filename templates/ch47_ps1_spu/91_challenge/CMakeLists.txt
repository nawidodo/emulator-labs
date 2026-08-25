add_executable(ch47_91_challenge_tests
    main.cpp
    ../01_adpcm/adpcm.cpp)
target_include_directories(ch47_91_challenge_tests PRIVATE ..)
target_link_libraries(ch47_91_challenge_tests PRIVATE labstest)
add_test(NAME ch47_91_challenge COMMAND ch47_91_challenge_tests)
