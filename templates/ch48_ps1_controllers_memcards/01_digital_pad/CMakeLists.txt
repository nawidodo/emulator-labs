add_executable(ch48_01_pad_tests main.cpp)
target_include_directories(ch48_01_pad_tests PRIVATE ..)
target_link_libraries(ch48_01_pad_tests PRIVATE labstest)
add_test(NAME ch48_01_pad COMMAND ch48_01_pad_tests)
