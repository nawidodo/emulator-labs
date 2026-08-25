add_executable(ch15_01_oam_tests main.cpp)
target_link_libraries(ch15_01_oam_tests PRIVATE labstest)
add_test(NAME ch15_01_oam COMMAND ch15_01_oam_tests)
