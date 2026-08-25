find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME ch01_03_generator
         COMMAND ${Python3_EXECUTABLE}
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_generator.py)
