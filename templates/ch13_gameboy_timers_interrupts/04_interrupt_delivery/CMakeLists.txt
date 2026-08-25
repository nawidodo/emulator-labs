add_executable(ch13_04_irq_tests main.cpp)
target_link_libraries(ch13_04_irq_tests PRIVATE labstest)
add_test(NAME ch13_04_irq COMMAND ch13_04_irq_tests)
