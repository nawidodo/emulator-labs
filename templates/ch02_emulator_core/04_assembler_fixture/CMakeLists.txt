add_executable(ch02_04_tests main.cpp)
target_link_libraries(ch02_04_tests PRIVATE labstest)
# Fixture files are read relative to the exercise source dir.
add_test(NAME ch02_04_unit COMMAND ch02_04_tests
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# Headless runner CLI (curriculum §52 shape).
add_executable(ch02_04_runner runner_main.cpp)
add_test(NAME ch02_04_runner_help COMMAND ch02_04_runner --help)
add_test(NAME ch02_04_runner_requires_rom COMMAND ch02_04_runner)
set_tests_properties(ch02_04_runner_requires_rom PROPERTIES WILL_FAIL TRUE)

# Golden-trace comparison through tools/labs/compare_trace.py. Only wired
# when the repo root (and its tools/) is discoverable from here — isolated
# verification trees skip these and rely on the unit tests above.
set(_ch02_root "${CMAKE_CURRENT_LIST_DIR}")
while(NOT EXISTS "${_ch02_root}/tools/labs/compare_trace.py")
  get_filename_component(_ch02_parent "${_ch02_root}" DIRECTORY)
  if(_ch02_parent STREQUAL _ch02_root)
    set(_ch02_root "")
    break()
  endif()
  set(_ch02_root "${_ch02_parent}")
endwhile()

if(_ch02_root)
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_EXECUTABLE)
    foreach(_prog countdown summem fib)
      string(REGEX REPLACE "summem" "sum_mem" _bin "${_prog}")
      add_test(
        NAME ch02_04_trace_${_prog}
        COMMAND ${Python3_EXECUTABLE} -c
          "import subprocess, sys; subprocess.check_call([sys.argv[1], '--rom', sys.argv[2], '--cycles', '400', '--trace', sys.argv[3]]); subprocess.check_call([sys.executable, sys.argv[4], sys.argv[5], sys.argv[3]])"
        $<TARGET_FILE:ch02_04_runner>
        "${CMAKE_CURRENT_SOURCE_DIR}/programs/${_bin}.bin"
        "${CMAKE_CURRENT_BINARY_DIR}/actual_${_prog}.log"
        "${_ch02_root}/tools/labs/compare_trace.py"
        "${_ch02_root}/tests/public/ch02_emulator_core/traces/${_prog}.log")
    endforeach()
  endif()
endif()
