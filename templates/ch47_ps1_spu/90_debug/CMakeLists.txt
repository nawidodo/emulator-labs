add_executable(ch47_90_debug_tests
    main.cpp
    ../01_adpcm/adpcm.cpp)
target_include_directories(ch47_90_debug_tests PRIVATE ..)
target_link_libraries(ch47_90_debug_tests PRIVATE labstest)
add_test(NAME ch47_90_debug COMMAND ch47_90_debug_tests)
