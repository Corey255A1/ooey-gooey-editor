# Part 2: Building the Foundation of an OOEY-GOOEY WYSIWYG Editor

With the architectural blueprint in hand, we are ready to implement the foundation of the visual builder. This post covers configuring the build system, linking the library dependencies, and designing the editor's visual shell using its own layout engine (eating our own dog food).

---

## 1. Project Organization and CMake Setup

The OOEY-GOOEY editor depends on three core static libraries built under the main `ooey` repository:
1. `libooey.a` - The core rendering and windowing engine.
2. `libgooey.a` - The C++ MVVM GUI element library.
3. `libtooey.a` - The parser, binder, and compiler utilities.

To build our editor, we create a new C++ project containing a standard layout:
```
ooey-gooey-editor/
├── CMakeLists.txt
├── docs/
│   └── blog/
└── src/
    ├── main.cpp
    ├── EditorView.ooey        (Layout file describing the editor's UI)
    ├── EditorViewModel.hpp    (Reactive state definition)
    ├── EditorViewModel.cpp
    └── DynamicInterpreter.hpp (Dynamic preview AST instantiator)
```

### Configuring `CMakeLists.txt`
The build configuration must locate system libraries (OpenGL, Vulkan, Wayland, X11) and link the imported static libraries. Crucially, it must configure a custom compile command that invokes the `tooey_compiler` binary to process `EditorView.ooey` whenever the layout is updated:

```cmake
cmake_minimum_required(VERSION 3.20)
project(ooey_gooey_editor LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable address sanitizer to match the precompiled libraries in the ooey repo
add_compile_options(-fsanitize=address)
add_link_options(-fsanitize=address)

# Find system libraries required by the windowing backend
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
    OpenGL::GL Vulkan::Vulkan X11::X11 ${PNG_LIBRARIES} Threads::Threads
    ${CMAKE_DL_LIBS} ${WAYLAND_LIBRARIES} ${WAYLAND_EGL_LIBRARIES} ${EGL_LIBRARIES} ${XKBCOMMON_LIBRARIES}
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

# Custom command to invoke the tooey_compiler
set(EDITOR_OOEY "${CMAKE_CURRENT_SOURCE_DIR}/src/EditorView.ooey")
set(EDITOR_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/editor_gen")
set(EDITOR_GEN_HDR "${EDITOR_GEN_DIR}/EditorView.hpp")
set(EDITOR_GEN_SRC "${EDITOR_GEN_DIR}/EditorView.cpp")

add_custom_command(
    OUTPUT "${EDITOR_GEN_HDR}" "${EDITOR_GEN_SRC}"
    COMMAND /home/corey/code/ooey/build/tooey/tooey_compiler "${EDITOR_OOEY}" "${EDITOR_GEN_DIR}" "EditorView" "EditorViewModel"
    DEPENDS "${EDITOR_OOEY}" "/home/corey/code/ooey/build/tooey/tooey_compiler"
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
```

---

## 2. Designing the Editor Layout (`EditorView.ooey`)

To construct the interface of the editor, we describe its visual hierarchy in the `.ooey` layout language. 

The editor requires a horizontal row containing three main panels: a left side sidebar (palette & visual tree), a center editing canvas and text area, and a right sidebar (property inspector).

```
HBox id=editorRoot spacing=10 width="MatchParent" height="MatchParent":
    VBox id=leftSidebar width=200 spacing=15:
        Label id=toolboxTitle text="Tool Palette"
        ListControl id=toolboxList items=@binding.toolboxItems width="MatchParent" height=160:
            Row id=toolRow height=35:
                Label id=toolLabel text=@binding.toolboxItems.name
                
        HBox id=toolActions spacing=10 width="MatchParent" height=35:
            Button id=btnAdd text="Add Selected"
            
        Label id=treeTitle text="Visual Tree Hierarchy"
        ListControl id=hierarchyList items=@binding.hierarchyItems width="MatchParent" height=180:
            Row id=hierRow height=35:
                Label id=hierLabel text=@binding.hierarchyItems.name

        HBox id=treeActions spacing=10 width="MatchParent" height=35:
            Button id=btnDelete text="Delete"
            Button id=btnMoveUp text="Up"
            Button id=btnMoveDown text="Down"

    VBox id=centerArea spacing=10 width=540 height="MatchParent":
        Label id=canvasTitle text="Live Preview Canvas"
        ScrollContainer id=canvasContainer width="MatchParent" height=420:
            CanvasLayout id=previewCanvas width=500 height=400
        
        Label id=editorTitle text="DSL Code Editor"
        RichTextBox id=codeEditor text=@binding.dslText width="MatchParent" height=240

    VBox id=rightSidebar width=200 spacing=15:
        Label id=propertiesTitle text="Property Inspector"
        ListControl id=propertiesList items=@binding.propertyItems width="MatchParent" height=350:
            Row id=propRow height=35:
                Label id=propName text=@binding.propertyItems.name width=90
                TextBox id=propVal text=@binding.propertyItems.value width=90
```

### Structural Highlights:
1. **Interactive Lists**: `ListControl` components bind to `toolboxItems`, `hierarchyItems`, and `propertyItems` arrays defined in the view model.
2. **The Preview Canvas**: Nested inside a `ScrollContainer` is a `CanvasLayout`. This is the visual stage where dynamic preview widgets are constructed.
3. **The RichText Editor**: A `RichTextBox` bound to the reactive `dslText` string. This displays the raw code.

---

## 3. Creating the State Container (`EditorViewModel`)

The view model serves as the glue holding the editor's reactive state. It exposes observable properties (`Property<T>`) for the toolbox registry, active document hierarchy, properties of the selected element, and the raw text representation.

Here is the declaration structure:

```cpp
#pragma once
#include "gooey/mvvmc/property.hpp"
#include "gooey/mvvmc/subscription_sink.hpp"
#include "tooey/ast.hpp"
#include <string>
#include <vector>
#include <memory>

struct ToolboxItem { std::string name; };
struct HierarchyItem { std::string name; int indent; std::string id; };
struct PropertyItem { std::string name; std::string value; };

class EditorViewModel : public gooey::mvvmc::ViewModel {
public:
    EditorViewModel();

    // Observable properties bound by the Editor UI
    gooey::mvvmc::Property<std::vector<ToolboxItem>> toolboxItems;
    gooey::mvvmc::Property<std::vector<HierarchyItem>> hierarchyItems;
    gooey::mvvmc::Property<std::vector<PropertyItem>> propertyItems;
    gooey::mvvmc::Property<std::string> dslText;

    int selectedIndex = -1;
    bool is_updating_ = false;

    // View Model actions
    void selectElement(int index);
    void updateProperty(int index, const std::string& newValue);
    void addControlToCanvas(const std::string& type);
    void updateCanvasFromDsl(const std::string& dsl);
    void deleteSelectedElement();
    void moveSelectedElementUp();
    void moveSelectedElementDown();

    // The document AST representation
    std::shared_ptr<tooey::AstNode> currentAst;

private:
    gooey::mvvmc::SubscriptionSink sink_;
    void rebuildHierarchyItems(const std::shared_ptr<tooey::AstNode>& node, int indent);
    void updatePropertyItems();
    std::string generateDslFromAst(const std::shared_ptr<tooey::AstNode>& node, int indent);
};
```

---

## 4. Bootstrapping the Main Loop (`main.cpp`)

The application entry point handles initializing the graphics window backend, constructing the view model and views, and running the message loop.

```cpp
#include <iostream>
#include <memory>
#include "gooey/application.hpp"
#include "ooey/platform.hpp"
#include "EditorViewModel.hpp"
#include "EditorView.hpp"

int main() {
    std::cout << "Bootstrapping OOEY-GOOEY WYSIWYG Editor...\n";

    gooey::Application app;
    auto backend = ooey::create_default_window_backend();
    
    // Create editor window at 1024x768 (fits sidebars and 500px canvas)
    if (!backend || !backend->create({1024, 768}, "OOEY-GOOEY Visual Designer")) {
        std::cerr << "Failed to initialize graphics window backend\n";
        return 1;
    }
    app.set_window_backend(std::move(backend));

    auto viewModel = std::make_shared<EditorViewModel>();
    auto editorView = std::make_shared<EditorView>(viewModel);

    // Apply dark UI palette styling
    app.set_clear_color(ooey::Color{22, 22, 28});
    editorView->set_padding(15);
    app.set_root_view(editorView);

    app.run();
    return 0;
}
```

In the next post, we will build the core engine of the visual designer: the Live-Reload Pipeline, which compiles layout code text into active, interactable widgets on the fly.
