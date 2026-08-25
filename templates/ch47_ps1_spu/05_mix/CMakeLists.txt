add_executable(ch47_05_mix_tests
    main.cpp
    ../01_adpcm/adpcm.cpp)
target_include_directories(ch47_05_mix_tests PRIVATE ..)
target_link_libraries(ch47_05_mix_tests PRIVATE labstest)
add_test(NAME ch47_05_mix COMMAND ch47_05_mix_tests)

add_executable(ch47_05_spu_runner runner_main.cpp ../01_adpcm/adpcm.cpp)
target_link_libraries(ch47_05_spu_runner PRIVATE labstest)
add_test(NAME ch47_05_spu_runner_help COMMAND ch47_05_spu_runner --help)
