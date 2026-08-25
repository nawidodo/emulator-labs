add_executable(ch42_01_texture_pages_tests tests.cpp tex_stages.hpp)
target_link_libraries(ch42_01_texture_pages_tests PRIVATE labstest)
add_test(NAME ch42_01_texture_pages COMMAND ch42_01_texture_pages_tests)

add_executable(ch42_01_texture_pages_runner runner_main.cpp tex_stages.hpp)
add_test(NAME ch42_01_texture_pages_runner_help
         COMMAND ch42_01_texture_pages_runner --help)
