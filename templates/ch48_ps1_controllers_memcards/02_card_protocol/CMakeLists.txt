add_executable(ch48_02_card_tests main.cpp)
target_include_directories(ch48_02_card_tests PRIVATE ..)
target_link_libraries(ch48_02_card_tests PRIVATE labstest)
add_test(NAME ch48_02_card COMMAND ch48_02_card_tests)
