add_executable(ch29_04_irq_tests main.cpp)
target_link_libraries(ch29_04_irq_tests PRIVATE labstest)
add_test(NAME ch29_04_irq COMMAND ch29_04_irq_tests)
