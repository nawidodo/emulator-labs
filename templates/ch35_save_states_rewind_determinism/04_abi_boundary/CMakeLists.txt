add_executable(ch35_04_abi_tests main.cpp core.cpp)
target_link_libraries(ch35_04_abi_tests PRIVATE labstest)
target_include_directories(ch35_04_abi_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME ch35_04_abi COMMAND ch35_04_abi_tests)
