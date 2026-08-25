add_executable(ch47_01_adpcm_tests
    main.cpp
    adpcm.cpp)
target_include_directories(ch47_01_adpcm_tests PRIVATE ..)
target_link_libraries(ch47_01_adpcm_tests PRIVATE labstest)
add_test(NAME ch47_01_adpcm COMMAND ch47_01_adpcm_tests)
