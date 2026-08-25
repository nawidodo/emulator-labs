add_executable(i8080_challenge_tests main.cpp ${CMAKE_CURRENT_SOURCE_DIR}/cpu.cpp)
target_link_libraries(i8080_challenge_tests PRIVATE labstest)
target_include_directories(i8080_challenge_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME ch07_91_challenge COMMAND i8080_challenge_tests)
