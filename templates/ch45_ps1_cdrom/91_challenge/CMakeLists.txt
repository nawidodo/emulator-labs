add_executable(ch45_91_session_tests main.cpp)
target_link_libraries(ch45_91_session_tests PRIVATE labstest)
add_test(NAME ch45_91_session COMMAND ch45_91_session_tests)

add_executable(ch45_cd_cli cd_cli.cpp)
add_test(NAME ch45_cd_cli_help COMMAND ch45_cd_cli)
