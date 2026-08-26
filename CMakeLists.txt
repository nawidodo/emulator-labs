cmake_minimum_required(VERSION 3.21)
# Language policy (decided — see docs/AUTHORING.md):
#   C   → strict ISO C17: alternate foundations track cores
#         (tracks/foundations-c17); becomes the learner core language
#         when the staged migration lands
#   CXX → ISO C++20: current executable course + tests/tooling
project(emulator-labs LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LABS_WARNINGS "Enable aggressive warnings" On)
option(LABS_SANITIZE "Build with ASan+UBSan" Off)

if(LABS_SANITIZE)
  if(MSVC)
    # MSVC: ASan via /fsanitize=address; UBSan has no direct switch.
    add_compile_options(/fsanitize=address)
  else()
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
  endif()
endif()

if(LABS_WARNINGS)
  if(MSVC)
    add_compile_options(/W4)
  else()
    add_compile_options(-Wall -Wextra -Wpedantic)
  endif()
endif()

add_subdirectory(third_party/labstest)
enable_testing()
# Skeleton and solution trees are built exclusively: pick one per configure.
option(LABS_BUILD_SOLUTIONS "Build reference solutions instead of skeletons" Off)
set(LABS_ROOT "skels")
if(LABS_BUILD_SOLUTIONS)
  set(LABS_ROOT "solutions")
endif()
file(GLOB lab_cmake_lists RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
     "${LABS_ROOT}/ch*/CMakeLists.txt"
     "${LABS_ROOT}/ch*/*/CMakeLists.txt")
foreach(lab ${lab_cmake_lists})
  get_filename_component(lab_dir "${lab}" DIRECTORY)
  add_subdirectory("${lab_dir}")
endforeach()
if(LABS_BUILD_SOLUTIONS)
  add_subdirectory(tools/labs/nes_gate)
  add_subdirectory(tools/labs/ps1_gate)
endif()
