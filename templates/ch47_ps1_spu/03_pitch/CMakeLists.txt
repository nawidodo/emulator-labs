add_executable(ch47_03_pitch_tests
    main.cpp
    ../01_adpcm/adpcm.cpp)
target_include_directories(ch47_03_pitch_tests PRIVATE ..)
target_link_libraries(ch47_03_pitch_tests PRIVATE labstest)
add_test(NAME ch47_03_pitch COMMAND ch47_03_pitch_tests)
