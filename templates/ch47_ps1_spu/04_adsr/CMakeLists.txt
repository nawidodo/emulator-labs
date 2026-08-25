add_executable(ch47_04_adsr_tests main.cpp)
target_include_directories(ch47_04_adsr_tests PRIVATE ..)
target_link_libraries(ch47_04_adsr_tests PRIVATE labstest)
add_test(NAME ch47_04_adsr COMMAND ch47_04_adsr_tests)
