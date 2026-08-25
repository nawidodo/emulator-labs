add_executable(ch43_ct_dma_tests main.cpp)
target_link_libraries(ch43_ct_dma_tests PRIVATE labstest)
add_test(NAME ch43_ct_dma COMMAND ch43_ct_dma_tests)
