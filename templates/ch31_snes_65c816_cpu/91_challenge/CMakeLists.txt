add_executable(ch31_91_challenge_tests main.cpp)
target_link_libraries(ch31_91_challenge_tests PRIVATE labstest)
target_compile_definitions(ch31_91_challenge_tests PRIVATE
    CHALLENGE_SRC_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME ch31_91_challenge COMMAND ch31_91_challenge_tests)
