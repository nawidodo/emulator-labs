add_executable(ch42_02_blend_clip_tests tests.cpp blend_stages.hpp)
target_link_libraries(ch42_02_blend_clip_tests PRIVATE labstest)
add_test(NAME ch42_02_blend_clip COMMAND ch42_02_blend_clip_tests)

add_executable(ch42_02_blend_clip_runner runner_main.cpp blend_stages.hpp)
add_test(NAME ch42_02_blend_clip_runner_help
         COMMAND ch42_02_blend_clip_runner --help)
