add_executable(ch10_ldh_tests main.cpp)
target_link_libraries(ch10_ldh_tests PRIVATE labstest)
add_test(NAME ch10_ldh COMMAND ch10_ldh_tests)

add_executable(ch10_ldh_runner runner_main.cpp)
