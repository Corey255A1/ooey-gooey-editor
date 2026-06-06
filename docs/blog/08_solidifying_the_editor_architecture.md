# Part 8: Solidifying the Editor Architecture with SOLID Principles

As software grows, maintaining code quality is critical to preventing architectural rot. A codebase that starts as a simple prototype can quickly become a "spaghetti" of coupled components. In this post, we evaluate the architecture of the **OOEY-GOOEY WYSIWYG Editor**, identify clear violations of the **SOLID principles**, and walk through our refactoring process to build a robust, maintainable, and extensible system.

---

## The Initial Architectural Diagnosis

Our primary focus was `main.cpp`, which grew to nearly 400 lines and accumulated several distinct responsibilities:
1. **Windowing & Application Bootstrapping**: Setting up the Wayland/GL window backend and starting the main event loop.
2. **Event Routing & Binding**: Connecting ViewModel actions (add element, delete element, selections) to the UI View.
3. **Canvas Selection & Outlines**: Defining `SelectionOverlay` inline to draw highlight borders and resizing grips.
4. **Canvas Hit-Testing**: Carrying out recursive UI coordinates checks to translate clicks into element selections.
5. **Code Diagnostics (Linting)**: Performing lexer checks, duplicate ID checks, unrecognized tag checks, and missing localization audits inside nested inline structures.

This monolithic layout was highly fragile, difficult to unit-test, and made adding new widgets or custom rules extremely labor-intensive. Here is how we applied each of the **SOLID** principles to resolve these architectural issues.

---

## 1. Single Responsibility Principle (SRP)

> *"A class should have one, and only one, reason to change."*

### The Violations
`main.cpp` was acting as a bootstrap script, a UI component library, a coordinate mathematics library, and a multi-pass static code analyzer all at once. 

### The Refactoring
We extracted three distinct, single-purpose classes into their own dedicated files (header and implementation separation):
1. **[SelectionOverlay](file:///home/corey/code/ooey-gooey-editor/src/SelectionOverlay.hpp)**: A dedicated custom element responsible solely for drawing the outline and grips over the selected canvas widget.
2. **[HitTester](file:///home/corey/code/ooey-gooey-editor/src/HitTester.hpp)**: A stateless utility containing the recursive geometric hit-testing logic for mapping pointer coordinate clicks to layout elements.
3. **[Linter](file:///home/corey/code/ooey-gooey-editor/src/Linter.hpp)**: A dedicated service class containing the multi-pass compilation and syntax verification rules for the OOEY DSL.

By separating these concerns, the responsibilities are cleanly distributed, and changing the look of selection grips or updating a validation rule no longer runs the risk of breaking window startup or event bindings.

---

## 2. Open-Closed Principle (OCP)

> *"Software entities should be open for extension, but closed for modification."*

### The Violations
The linter check for unrecognized controls had a hardcoded list of supported tag names:
```cpp
static const std::set<std::string> known_types = {
    "VBox", "Column", "HBox", "Row", "Grid", "FlowLayout",
    "Button", "CheckBox", "Label", "TextBox", "RichTextBox", ...
};
```
If a developer created a new custom control or layout type, they had to directly modify the core checking loop inside `main.cpp` to prevent the editor from warning about "unrecognized types."

### The Refactoring
We refactored this checker in the new **[Linter](file:///home/corey/code/ooey-gooey-editor/src/Linter.hpp)** class to support dynamic runtime registration. The list of recognized tags is initialized with sensible defaults but can be dynamically extended:

```cpp
class Linter {
public:
    static std::vector<Diagnostic> run_diagnostics(const std::string& dsl);
    static void register_known_type(const std::string& type_name);
};
```
Now, if a plugin registers a new widget, it simply invokes `Linter::register_known_type("MyWidget")`. The linter immediately recognizes it as valid without modifying a single line of the core compiler or editor codebase.

---

## 3. Liskov Substitution Principle (LSP)

> *"Objects in a program should be replaceable with subtypes without altering correctness."*

### The Violations
Throughout the codebase, base interfaces are bypassed in favor of concrete casts. For instance:
```cpp
dynamic_cast<gooey::mvvmc::Controller*>(Application::get_instance()->get_controller())->set_focused_element(content_view_);
```
Here, the code bypasses `IController` because it lacks focus management features. If a different controller subtype (e.g. `UnitTestingController` or `HeadlessController`) were substituted, the cast would return `nullptr`, causing immediate crashes or failed events.

### The Refactoring Analysis
To adhere to LSP, interface contracts must be robust enough to handle subtype variations. In this scenario, we identified that `IController` should be extended in the UI toolkit to natively support focus management. For the editor's immediate classes, we ensured that subtype expectations are consistent. For example, `SelectionOverlay` inherits from `GooeyElement` and implements `draw()` strictly conforming to the base class lifecycle, without relying on down-casting inside the drawing pass.

---

## 4. Interface Segregation Principle (ISP)

> *"Many client-specific interfaces are better than one general-purpose interface."*

### The Refactoring
We designed the **[Linter](file:///home/corey/code/ooey-gooey-editor/src/Linter.hpp)** class with a thin, highly specialized interface:
```cpp
namespace editor {
struct Diagnostic {
    int line;
    int start_col;
    int end_col;
    ooey::Color color;
};

class Linter {
public:
    static std::vector<Diagnostic> run_diagnostics(const std::string& dsl);
};
}
```
The client (the code editor) does not need to know about parser internals, tokenization logic, or AST node structures. It only consumes a clean list of `Diagnostic` ranges. This is separate from any text buffer modification or display mechanisms. The linter has no dependency on UI render targets, and the editor can render the diagnostics as squiggles, output logs, or tooltip popups.

---

## 5. Dependency Inversion Principle (DIP)

> *"Depend upon abstractions, not concretes."*

### The Violations
The inline missing-localization checker inside `main.cpp` was directly querying the UI class `RichTextBox` for line text:
```cpp
std::string line_text = editor.get_line_text(n->line - 1);
```
This created a direct coupling from compile-time/static analysis logic to active UI layout elements. If the code buffer was held in a file or string instead of a `RichTextBox` control, this logic could not be executed.

### The Refactoring
We inverted the dependency by passing the raw DSL text block (`const std::string& dsl`) directly into `Linter::run_diagnostics()`. The linter splits the text into line segments internally:
```cpp
std::vector<std::string> lines;
std::string line;
std::stringstream ss(dsl);
while (std::getline(ss, line)) {
    lines.push_back(line);
}
```
The localization logic now operates strictly on standard library strings and coordinates, removing all dependencies on the GUI widgets. We can now test the linter in headless environments, command-line scripts, or unit-tests without instantiating a single window or UI element.

---

## Architectural Results

The refactored `main.cpp` is now a highly readable, 200-line bootstrap routine focusing strictly on event bindings and application configuration. 

```
 [ main.cpp ] (Boots window & links events)
      │
      ├───► [ Linter ] ──────► Returns logical Diagnostics (decoupled from UI)
      ├───► [ HitTester ] ───► Stateless coordinates checking
      └───► [ SelectionOverlay ] ──► Encapsulated outline/grips drawing
```

This clean separation of concerns makes our codebase ready for features like dynamic widget plugins, remote collaboration, and headless automated testing.
