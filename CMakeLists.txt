cmake_minimum_required(VERSION 3.20)
project(ooey_gooey_editor LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable address sanitizer to match the precompiled libraries in the ooey repo
add_compile_options(-fsanitize=address)
add_link_options(-fsanitize=address)


# Find system packages required by ooey/gooey
find_package(OpenGL REQUIRED)
find_package(Vulkan REQUIRED)
find_package(X11 REQUIRED)
find_package(PNG REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(WAYLAND REQUIRED wayland-client)
pkg_check_modules(WAYLAND_EGL REQUIRED wayland-egl)
pkg_check_modules(EGL REQUIRED egl)
pkg_check_modules(XKBCOMMON REQUIRED xkbcommon)

# Define imported target for ooey
add_library(ooey STATIC IMPORTED)
set_target_properties(ooey PROPERTIES
    IMPORTED_LOCATION "/home/corey/code/ooey/build/libooey.a"
    INTERFACE_INCLUDE_DIRECTORIES "/home/corey/code/ooey/ooey/include"
)
target_link_libraries(ooey INTERFACE
    OpenGL::GL
    Vulkan::Vulkan
    X11::X11
    ${PNG_LIBRARIES}
    Threads::Threads
    ${CMAKE_DL_LIBS}
    ${WAYLAND_LIBRARIES}
    ${WAYLAND_EGL_LIBRARIES}
    ${EGL_LIBRARIES}
    ${XKBCOMMON_LIBRARIES}
)


# Define imported target for gooey
add_library(gooey STATIC IMPORTED)
set_target_properties(gooey PROPERTIES
    IMPORTED_LOCATION "/home/corey/code/ooey/build/libgooey.a"
    INTERFACE_INCLUDE_DIRECTORIES "/home/corey/code/ooey/gooey/include"
)
target_link_libraries(gooey INTERFACE ooey)

# Define imported target for tooey
add_library(tooey STATIC IMPORTED)
set_target_properties(tooey PROPERTIES
    IMPORTED_LOCATION "/home/corey/code/ooey/build/tooey/libtooey.a"
    INTERFACE_INCLUDE_DIRECTORIES "/home/corey/code/ooey/tooey/include"
)

# Generate EditorView.hpp and EditorView.cpp from EditorView.ooey using pre-compiled compiler
set(EDITOR_OOEY "${CMAKE_CURRENT_SOURCE_DIR}/src/EditorView.ooey")
set(EDITOR_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/editor_gen")
set(EDITOR_GEN_HDR "${EDITOR_GEN_DIR}/EditorView.hpp")
set(EDITOR_GEN_SRC "${EDITOR_GEN_DIR}/EditorView.cpp")

add_custom_command(
    OUTPUT "${EDITOR_GEN_HDR}" "${EDITOR_GEN_SRC}"
    COMMAND /home/corey/code/ooey/build/tooey/tooey_compiler "${EDITOR_OOEY}" "${EDITOR_GEN_DIR}" "EditorView" "EditorViewModel"
    DEPENDS "${EDITOR_OOEY}"
    COMMENT "Compiling EditorView.ooey with pre-compiled tooey_compiler"
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

