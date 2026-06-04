# OOEY-GOOEY WYSIWYG Editor: Architectural Specification

This document lays out the architecture of the OOEY-GOOEY Editor, a live visual and text-based editor for the OOEY visual markup language (`.ooey` files). It analyzes design paradigms from industry-standard UI builders and specifies how the editor utilizes the `tooey` and `gooey` libraries to deliver a seamless design workflow.

---

## 1. Design Inspirations: Visual UI Builders

Modern GUI builders employ various paradigms to facilitate rapid UI prototyping:

### A. QtWidgets (Qt Creator / Designer)
* **Paradigm**: Static template code generation.
* **Layout Model**: Absolute coordinates or container-managed layouts (`QVBoxLayout`, `QHBoxLayout`, `QGridLayout`).
* **Output Format**: XML-based `.ui` files.
* **Build Integration**: The User Interface Compiler (`uic`) compiles `.ui` files into standard C++ headers (`ui_mainwindow.h`) at compile time.
* **Visual Editing**: Drag-and-drop from a toolbox, a tree-based Object Inspector for the layout hierarchy, and a Property Editor grid.

### B. Windows Forms (Visual Studio)
* **Paradigm**: Direct code-behind serialization.
* **Layout Model**: Coordinate-based anchoring and docking (`Anchor`, `Dock`).
* **Output Format**: Native C++ or C# source code (e.g. `Form1.Designer.cs`).
* **Visual Editing**: Drag-and-drop UI where actions on the canvas directly serialize into C# statements inside the compiler-managed `InitializeComponent()` block.

### C. WPF / Blend (XAML)
* **Paradigm**: Dynamic markup with reactive split-view.
* **Layout Model**: Grid-based row/column definitions, alignment margins, and auto-sizing.
* **Output Format**: XML-based Extensible Application Markup Language (`.xaml`).
* **Visual Editing**: Split-screen showing the raw XAML source and the visual preview simultaneously. Changes on the canvas write XAML; changes in the text editor update the canvas via a real-time layout scanner.

---

## 2. Editor Design Paradigm: The Split-View Interactive Workspace

The OOEY-GOOEY Editor adopts the **split-screen interactive design model** (inspired by WPF/Blend) to ensure developers have full control over the code representation while enjoying immediate visual feedback.

```
┌────────────────────────────────────────────────────────┐
│                   OOEY-GOOEY EDITOR                    │
├───────────────────┬────────────────────────────────────┤
│ 1. Tool Palette   │ 3. Live WYSIWYG Canvas             │
│  [Label] [Button] │  ┌──────────────────────────────┐  │
│  [VBox]  [HBox]   │  │    Welcome to OOEY!          │  │
│                   │  │    [Click Me]                │  │
│ 2. Visual Tree    │  └──────────────────────────────┘  │
│  ▼ VBox           ├────────────────────────────────────┤
│    Label          │ 4. Property Inspector              │
│    Button         │    ID:    btnSubmit                │
│                   │    Text:  Submit                   │
├───────────────────┴────────────────────────────────────┤
│ 5. Split Text Editor & Standalone Linter (Diagnostics) │
│    1: VBox id=rootLayout:                              │
│    2:     Label text="Welcome to OOEY!"                │
│    3:     Button id=btnSubmit text="Submit"            │
└────────────────────────────────────────────────────────┘
```

The interface is divided into five core panels:
1. **Tool Palette**: A list of standard C++ GUI elements (such as `Button`, `Label`, `TextBox`, `CheckBox`, `ListControl`) and container layouts that can be placed on the canvas.
2. **Visual Tree (Hierarchy Inspector)**: A tree-representation of the parsed AST structure, illustrating parent-child constraints and facilitating drag-and-drop nesting.
3. **Live WYSIWYG Canvas**: An interactive render area that displays the layout in real time. It supports selection borders and resizing handles.
4. **Property Inspector**: A configuration grid mapping the properties (like `text`, `width`, `height`, `spacing`, bindings, signals) of the selected AST node to interactive input controls.
5. **DSL Code Editor**: A built-in code editor displaying the raw `.ooey` layout file, featuring syntax highlighting, line numbers, error markers, and code folding.

---

## 3. System Architecture & Live-Reload Pipeline

To enable zero-compilation real-time rendering, the editor runs a live parsing pipeline. Since compile-time generation of C++ headers requires system compilation (too slow for live editing), the editor interprets the layout tree dynamically.

```
                  ┌──────────────────────┐
                  │    DSL Code Buffer   │
                  └──────────┬───────────┘
                             │ (on keypress / update)
                             ▼
                  ┌──────────────────────┐
                  │ Lexer & Parser (AST) │
                  └──────────┬───────────┘
                             │
                             ▼
                  ┌──────────────────────┐
                  │ Standalone Linter    ├──────► [ Diagnostics Console ]
                  └──────────┬───────────┘
                             │ (if AST is valid)
                             ▼
                  ┌──────────────────────┐
                  │  Dynamic Interpreter │
                  └──────────┬───────────┘
                             │ (reconstructs nodes)
                             ▼
                  ┌──────────────────────┐
                  │ Dynamic GUI Tree     │
                  │ (GooeyElement list)  │
                  └──────────┬───────────┘
                             │ (adds to root)
                             ▼
                  ┌──────────────────────┐
                  │ Live Visual Canvas   │
                  └──────────────────────┘
```

### Components

1. **The Code Buffer**: The primary source of truth. It is updated either by manual typing in the DSL Code Editor or by visual modifications on the canvas/property editor.
2. **Parser & Lexer ([tooey](file:///home/corey/code/ooey/tooey))**: Tokenizes and parses the layout DSL into a `std::shared_ptr<AstNode>` tree.
3. **Standalone Linter**: Runs diagnostic checks on the AST to verify layout correctness, matching braces/indentations, duplicate IDs, missing localization keys, and incorrect types.
4. **Dynamic Interpreter**: Traverses the `AstNode` tree and dynamically instantiates equivalent runtime widgets using factory creators:
   - If node is `"Button"`, creates a `gooey::controls::Button`.
   - Maps parsed properties (`p_val.rawData`) directly using setters (e.g. `set_text`, `set_width`).
5. **Preview Canvas**: A container widget that hosts the interpreted `GooeyElement` tree.

---

## 4. Expanding the `tooey` Library: Standalone Linter Tool

To support IDE diagnostics (e.g., VSCode integration), we implement a standalone command-line linter tool named `tooey_lint` in the `tooey` project.

### Lint Checks
* **Syntax Validation**: Reports invalid indentation, unbalanced quotes, colons, or unrecognized characters.
* **Duplicate ID Detection**: Detects if multiple nodes share the same ID (which would cause compiler errors in the generated C++ files).
* **Missing Translation Keys**: Identifies `@tr("key")` keys that are missing from `localization.hpp` or target translation dictionaries.
* **Component Type Verification**: Flags undeclared custom includes or unrecognized control types.

### Editor Diagnostics Output Format
To integrate with typical IDE problem matchers, the linter prints diagnostics in standard GNU compiler format:
```
<filename>:<line>:<column>: <severity>: <message>
```
Example:
```
/home/corey/code/layout.ooey:3:12: error: Duplicate element ID 'btnSubmit' defined
/home/corey/code/layout.ooey:5:22: warning: Localization key 'non_existent_key' is missing from localization.hpp
```

---

## 5. Implementation Roadmap

1. **Linter Executable**: Develop `tooey_lint` in `tooey/src/linter_main.cpp` and update `tooey/CMakeLists.txt` to compile it.
2. **Editor Project Setup**: Initialize `ooey-gooey-editor` with CMake, linking against `ooey`, `gooey`, and `tooey` libraries.
3. **Interpreter Engine**: Implement the `DynamicInterpreter` mapping `AstNode` to live `GooeyElement` trees.
4. **WYSIWYG Layout**: Define the UI of the editor in `.ooey` layout files and compile them into C++ classes.
5. **Canvas Selection Handles**: Build custom controls representing outline selection borders and size drag grips to enable visual layout editing.
