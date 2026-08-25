add_executable(ch47_02_voice_tests
    main.cpp
    ../01_adpcm/adpcm.cpp)
target_include_directories(ch47_02_voice_tests PRIVATE ..)
target_link_libraries(ch47_02_voice_tests PRIVATE labstest)
add_test(NAME ch47_02_voice COMMAND ch47_02_voice_tests)
