add_executable(ch22_03_sprites_tests main.cpp)
target_link_libraries(ch22_03_sprites_tests PRIVATE labstest)
add_test(NAME ch22_03_sprites COMMAND ch22_03_sprites_tests)

add_executable(ch22_03_sprites_runner runner_main.cpp)
add_test(NAME ch22_03_sprites_runner_help
         COMMAND ch22_03_sprites_runner --help)
