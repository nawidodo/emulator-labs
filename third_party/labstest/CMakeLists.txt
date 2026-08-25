# Header-only deterministic test framework used by every chapter.
add_library(labstest INTERFACE)
target_include_directories(labstest INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
