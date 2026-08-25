add_executable(ch11_03_irq_tests main.cpp)
target_link_libraries(ch11_03_irq_tests PRIVATE labstest)
add_test(NAME ch11_03_irq COMMAND ch11_03_irq_tests)
