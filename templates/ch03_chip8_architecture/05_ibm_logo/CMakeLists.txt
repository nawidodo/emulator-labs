add_executable(ch03_05_logo_tests main.cpp)
target_link_libraries(ch03_05_logo_tests PRIVATE labstest)
add_test(NAME ch03_05_logo COMMAND ch03_05_logo_tests)

add_executable(ch03_05_logo_runner runner_main.cpp)
add_test(NAME ch03_05_logo_runner_help COMMAND ch03_05_logo_runner --help)
add_test(NAME ch03_05_logo_runner_render
         COMMAND ch03_05_logo_runner
                 --rom ${CMAKE_CURRENT_SOURCE_DIR}/roms/ibm_logo.ch8
                 --cycles 20 --hash-frame ${CMAKE_CURRENT_BINARY_DIR}/logo.rgba)
