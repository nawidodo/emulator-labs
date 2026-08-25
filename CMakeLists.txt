cmake_minimum_required(VERSION 3.21)
project(emulator-labs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LABS_WARNINGS "Enable aggressive warnings" On)
option(LABS_SANITIZE "Build with ASan+UBSan" Off)

if(LABS_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

if(LABS_WARNINGS)
  add_compile_options(-Wall -Wextra -Wpedantic)
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
