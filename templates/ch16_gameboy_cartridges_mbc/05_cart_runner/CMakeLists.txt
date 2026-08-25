add_executable(ch16_05_cart_runner_tests main.cpp)
target_link_libraries(ch16_05_cart_runner_tests PRIVATE labstest)
add_test(NAME ch16_05_cart COMMAND ch16_05_cart_runner_tests)

add_executable(ch16_05_cart_runner runner_main.cpp)
add_test(NAME ch16_05_cart_runner_help COMMAND ch16_05_cart_runner --help)
