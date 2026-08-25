add_executable(ch28_02_text_tests main.cpp)
target_link_libraries(ch28_02_text_tests PRIVATE labstest)
add_test(NAME ch28_02_text COMMAND ch28_02_text_tests)
