add_executable(ch29_01_dma_tests main.cpp)
target_link_libraries(ch29_01_dma_tests PRIVATE labstest)
add_test(NAME ch29_01_dma COMMAND ch29_01_dma_tests)
