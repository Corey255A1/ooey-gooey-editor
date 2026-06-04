cmake_minimum_required(VERSION 3.20)
project(ooey_gooey_editor LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add ooey project as a subdirectory to gain access to libraries and compiler
add_subdirectory(/home/corey/code/ooey ${CMAKE_BINARY_DIR}/ooey_build)

# Generate EditorView.hpp and EditorView.cpp from EditorView.ooey
set(EDITOR_OOEY "${CMAKE_CURRENT_SOURCE_DIR}/src/EditorView.ooey")
set(EDITOR_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/editor_gen")
set(EDITOR_GEN_HDR "${EDITOR_GEN_DIR}/EditorView.hpp")
set(EDITOR_GEN_SRC "${EDITOR_GEN_DIR}/EditorView.cpp")

add_custom_command(
    OUTPUT "${EDITOR_GEN_HDR}" "${EDITOR_GEN_SRC}"
    COMMAND tooey_compiler "${EDITOR_OOEY}" "${EDITOR_GEN_DIR}" "EditorView" "EditorViewModel"
    DEPENDS "${EDITOR_OOEY}" tooey_compiler
    COMMENT "Compiling EditorView.ooey with tooey_compiler"
)

add_executable(ooey_gooey_editor
    src/main.cpp
    src/EditorViewModel.cpp
    "${EDITOR_GEN_SRC}"
)

target_link_libraries(ooey_gooey_editor PRIVATE ooey gooey tooey)
target_include_directories(ooey_gooey_editor PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${EDITOR_GEN_DIR}"
)
