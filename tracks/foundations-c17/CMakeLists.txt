cmake_minimum_required(VERSION 3.21)
project(foundations-c17 LANGUAGES C)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Generated labs root (override for CI probes that generate elsewhere).
set(LABS_TEMPLATES_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/templates"
    CACHE PATH "generated chapter trees root")

add_compile_options(-pedantic-errors)
enable_testing()
file(GLOB labs_lists
     "${LABS_TEMPLATES_ROOT}/*/CMakeLists.txt"
     "${LABS_TEMPLATES_ROOT}/*/*/CMakeLists.txt")
foreach(l ${labs_lists})
  get_filename_component(d "${l}" DIRECTORY)
  get_filename_component(chname "${d}" NAME)
  # Generated trees live outside the source root: explicit binary dirs.
  add_subdirectory("${d}" "${CMAKE_CURRENT_BINARY_DIR}/${chname}")
endforeach()
