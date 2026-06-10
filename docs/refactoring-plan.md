# OOEY-GOOEY Editor: Refactoring Plan

**Version**: 1.0  
**Date**: 2026-06-10  
**Status**: Proposed

---

## Executive Summary

The `ooey-gooey-editor` has grown organically into a functional but poorly structured codebase. The primary symptom is a **849-line `main.cpp`** that conflates application bootstrap, UI wiring, widget instantiation, business logic, infrastructure configuration, and data transformation — all in one place. This document identifies every violation, proposes a clear modular target architecture, and provides concrete, file-by-file implementation steps with code patterns.

---

## Table of Contents

1. [Current Code Quality Audit](#1-current-code-quality-audit)
2. [Root Causes of Poor Structure](#2-root-causes-of-poor-structure)
3. [Target Architecture](#3-target-architecture)
4. [Proposed Module Breakdown](#4-proposed-module-breakdown)
5. [Design Patterns to Apply](#5-design-patterns-to-apply)
6. [SOLID Violations and Fixes](#6-solid-violations-and-fixes)
7. [File-by-File Refactoring Plan](#7-file-by-file-refactoring-plan)
8. [New Test Targets](#8-new-test-targets)
9. [Migration Strategy](#9-migration-strategy)
10. [Definition of Done](#10-definition-of-done)
11. [Appendix A: Current vs. Target Line Count](#appendix-a-current-vs-target-line-count-estimate)
12. [Appendix B: Naming Conventions](#appendix-b-naming-convention-for-new-files)

---

## 1. Current Code Quality Audit

### 1.1 `main.cpp` — The God File (849 lines)

`main.cpp` currently contains **7 distinct concerns** that should each live in separate modules:

| Responsibility | Lines (approx.) | Belongs In |
|---|---|---|
| `PropertyCell` widget class definition | 1–194 | `src/widgets/PropertyCell.hpp/.cpp` |
| `build_editor_menu_categories()` function | 196–358 | `src/menus/EditorMenuBuilder.hpp/.cpp` |
| Application/window bootstrap | 367–379 | `src/app/AppBootstrap.hpp/.cpp` |
| Theme manager creation & style registration | 386–422 | `src/theme/ThemeRegistry.hpp/.cpp` |
| Translation dictionary loading | 425–553 | `src/i18n/TranslationLoader.hpp/.cpp` |
| DataGrid column/cell wiring | 582–654 | `src/views/PropertyGridConfigurator.hpp/.cpp` |
| View ↔ ViewModel event binding | 655–811 | `src/controllers/EditorController.hpp/.cpp` |

**Quantitative debt**: 849 lines, 7 responsibilities, 0 unit tests, 0 interface abstractions in `main.cpp`.

### 1.2 `EditorViewModel.cpp` — Business Logic Leakage (355 lines)

The `EditorViewModel` handles:
- AST parsing coordination (should be in a `CanvasService` or `IDslCompilerService`)
- Undo/redo stack management (should be in a `HistoryManager`)
- Property-to-AST update mapping (should use injected `IAstMutator`)
- Hierarchy item construction (should be in a `HierarchyBuilder` or private helper extracted from VM)
- ID lookups via an ad-hoc `Finder` local struct (should use injected `IAstQuery`)

It also exposes `is_updating_` as a **public bool** (line 38 of `EditorViewModel.hpp`), leaking synchronization state to external callers (`main.cpp` line 761 reads it directly). This violates encapsulation.

### 1.3 `PropertyCell` — Embedded Anonymous Widget

`PropertyCell` is defined inside an anonymous namespace in `main.cpp`. This means:

- It **cannot be unit-tested** independently — it is invisible to any test file
- It **cannot be reused** in other contexts (e.g., a future standalone property panel)
- It directly calls `gooey::Application::get_instance()` — a **global singleton coupling** that prevents construction in a headless test environment
- Text color is **hardcoded** (`ooey::Color{240, 240, 240}`) — no theme awareness despite the rest of the app supporting themes
- Selection highlight color is **hardcoded** (`ooey::Color{0, 120, 215, 30}`) — will not adapt to Light, Lofi, or Cyberpunk themes

### 1.4 `AstService` — Static God Service

`AstService` has 6 static methods spanning 3 fundamentally different concerns:

- **Serialization** (`serialize_ast`) — converts AST to DSL text
- **Structural mutation** (`add_child_node`, `delete_node`, `move_node_up`, `move_node_down`) — modifies the AST tree
- **Property application** (`update_node_properties`) — modifies node properties
- **Query** (`find_node_by_id`) — reads the AST tree

All methods are `static`, making them:
- **Untestable in isolation** — no interface to mock; tests must build real `AstNode` trees
- **Impossible to substitute** — cannot inject a `MockAstSerializer` for unit testing `EditorViewModel`
- **Harder to extend** — a new serialization format (e.g., JSON export) requires modifying `AstService` itself, violating OCP

### 1.5 Translation Dictionaries — Magic Data in Code

Four `std::map<std::string, std::string>` objects containing ~130 translation entries each are embedded directly in `main()` (lines 426–553). Problems:

- **Hard to maintain**: Adding a new UI key requires editing `main.cpp` in 4 places simultaneously
- **No completeness verification**: No mechanism to detect that a key is in `en_US` but missing in `fr_FR`
- **Violates OCP**: Adding a new language requires modifying `main.cpp` — the composition root should not need to change for feature additions
- **Not localizable externally**: Translations cannot be loaded from external files/resources without rearchitecting

### 1.6 Missing Abstractions / Interfaces

Nothing in the codebase uses a pure virtual interface for:
- Canvas preview rendering (tightly coupled to `DynamicInterpreter::interpret` static call)
- DSL compilation (tightly coupled to `tooey::Lexer` / `tooey::Parser` called directly in `EditorViewModel`)
- History management (ad-hoc `std::vector<std::string>` in `EditorViewModel`)
- Theme configuration (raw `ThemeManager` API wired in `main()`)

This makes it impossible to:
- Swap implementations (e.g., a future `WasmDslCompilerService`)
- Mock dependencies for unit tests
- Extend behavior without modifying existing classes

### 1.7 Property Naming Inconsistency

`EditorViewModel.hpp` uses `snake_case` for members (`selectedIndex`, `is_updating_`) but `camelCase` for public properties (`toolboxItems`, `hierarchyItems`, `propertyItems`, `dslText`). The coding standards specify `snake_case` throughout. This inconsistency makes the codebase harder to read and violates `docs/coding-standards.md`.

---

## 2. Root Causes of Poor Structure

1. **No bootstrap/composition layer**: The `main()` function performs dependency injection manually, but without a structured composition root it has grown unbounded as features were added.

2. **MVVM without a Controller**: The editor has `EditorView` and `EditorViewModel` but no formal `EditorController` to wire them — the binding code leaked directly into `main()`.

3. **Anemic service layer**: `AstService` is a thin static wrapper. There is no service abstraction (`IAstMutator`, `IAstSerializer`) for inversion of control.

4. **No domain model layer**: `ToolboxItem`, `HierarchyItem`, `PropertyItem` are plain structs in `EditorViewModel.hpp`, mixed with ViewModel concerns. Domain types should have zero framework dependencies.

5. **No infrastructure layer**: Translation loading, theme registration, and window creation are interleaved in `main()` rather than isolated into dedicated infrastructure classes.

6. **No tests as forcing function**: The absence of unit tests meant there was no pressure to make code testable. Testability and good architecture are deeply correlated — making code testable naturally forces proper separation of concerns.

---

## 3. Target Architecture

The refactored architecture follows a **Layered MVVM+C architecture** (Model-View-ViewModel + Controller):

```
┌─────────────────────────────────────────────────────────────┐
│                       Presentation Layer                     │
│  EditorView.ooey (compiled) │ PropertyCell                  │
│  SelectionOverlay           │ PropertyGridConfigurator       │
├─────────────────────────────────────────────────────────────┤
│                       Controller Layer                       │
│  EditorController (wires View ↔ ViewModel ↔ Services)       │
│  EditorMenuBuilder (builds reactive menu categories)         │
├─────────────────────────────────────────────────────────────┤
│                       ViewModel Layer                        │
│  EditorViewModel (reactive Property<T> fields, commands)     │
│  (Delegates to: HistoryManager, injected services)          │
├─────────────────────────────────────────────────────────────┤
│                      Service / Domain Layer                  │
│  IAstSerializer / AstSerializer                              │
│  IAstMutator    / AstMutator                                 │
│  IAstQuery      / AstQuery                                   │
│  ICanvasPreviewService / CanvasPreviewService                │
│  IDslCompilerService   / DslCompilerService                  │
│  HistoryManager                                              │
├─────────────────────────────────────────────────────────────┤
│                     Infrastructure Layer                     │
│  ThemeRegistry (registers themes from config)                │
│  TranslationLoader (loads locale dictionaries)               │
│  AppBootstrap (creates window, wires Application)            │
└─────────────────────────────────────────────────────────────┘
```

### Proposed Directory Structure

```
src/
├── app/
│   └── AppBootstrap.hpp/.cpp             # Window creation, app lifecycle
├── controllers/
│   └── EditorController.hpp/.cpp         # Wires View ↔ ViewModel events
├── domain/
│   └── models.hpp                        # ToolboxItem, HierarchyItem, PropertyItem
├── i18n/
│   └── TranslationLoader.hpp/.cpp        # Loads locale dictionaries
├── menus/
│   └── EditorMenuBuilder.hpp/.cpp        # Builds MenuCategory vectors reactively
├── services/
│   ├── IAstSerializer.hpp                # Pure virtual: serialize AST → DSL text
│   ├── AstSerializer.hpp/.cpp            # Concrete serializer
│   ├── IAstMutator.hpp                   # Pure virtual: add/delete/move/update
│   ├── AstMutator.hpp/.cpp               # Concrete mutator
│   ├── IAstQuery.hpp                     # Pure virtual: find_by_id
│   ├── AstQuery.hpp/.cpp                 # Concrete query
│   ├── IDslCompilerService.hpp           # Pure virtual: compile + lint
│   ├── DslCompilerService.hpp/.cpp       # Concrete: wraps Lexer+Parser+Linter
│   └── HistoryManager.hpp/.cpp           # Undo/redo stack
├── theme/
│   └── ThemeRegistry.hpp/.cpp            # Registers all themes into ThemeManager
├── views/
│   ├── PropertyCell.hpp/.cpp             # Moved from anonymous namespace in main
│   └── PropertyGridConfigurator.hpp/.cpp # DataGrid column/cell wiring
├── viewmodels/
│   └── EditorViewModel.hpp/.cpp          # Slim MVVM ViewModel only
├── AstService.hpp/.cpp                   # (kept as thin deprecated facade briefly)
├── DynamicInterpreter.hpp/.cpp           # (unchanged, already self-contained)
├── SelectionOverlay.hpp/.cpp             # (unchanged, already self-contained)
└── main.cpp                              # Only: bootstrap + composition root (~60 lines)
```

---

## 4. Proposed Module Breakdown

### 4.1 `domain/models.hpp` — Pure Data Types

**Problem**: `ToolboxItem`, `HierarchyItem`, `PropertyItem` are defined inside `EditorViewModel.hpp`, coupling data types to the ViewModel include chain.

**Fix**: Extract to a standalone header with zero framework dependencies:

```cpp
// src/domain/models.hpp
#pragma once
#include <string>

namespace editor::domain {

struct ToolboxItem {
    std::string name;
};

struct HierarchyItem {
    std::string name;
    int         indent;
    std::string id;
};

struct PropertyItem {
    std::string name;
    std::string value;
};

} // namespace editor::domain
```

**Why this matters**: Domain types must be stable and dependency-free. This allows them to be used in service interfaces, unit tests, and ViewModels without pulling in `gooey` or `tooey` headers. A test file for `HistoryManager` should not need to include UI framework headers just to construct a `HierarchyItem`.

---

### 4.2 `services/` — Interface-Driven Service Layer

#### 4.2.1 `IAstSerializer.hpp`

```cpp
// src/services/IAstSerializer.hpp
#pragma once
#include "tooey/ast.hpp"
#include <string>
#include <memory>

namespace editor::services {

class IAstSerializer {
public:
    virtual ~IAstSerializer() = default;
    virtual std::string serialize(
        const std::shared_ptr<tooey::AstNode>& root,
        int indent = 0) const = 0;
};

} // namespace editor::services
```

#### 4.2.2 `IAstMutator.hpp`

```cpp
// src/services/IAstMutator.hpp
#pragma once
#include "tooey/ast.hpp"
#include <string>
#include <vector>
#include <utility>
#include <memory>

namespace editor::services {

class IAstMutator {
public:
    virtual ~IAstMutator() = default;

    virtual bool add_child(
        const std::shared_ptr<tooey::AstNode>& root,
        const std::string& parent_id,
        const std::shared_ptr<tooey::AstNode>& new_node) = 0;

    virtual bool delete_node(
        const std::shared_ptr<tooey::AstNode>& root,
        const std::string& target_id) = 0;

    virtual bool move_node_up(
        const std::shared_ptr<tooey::AstNode>& root,
        const std::string& target_id) = 0;

    virtual bool move_node_down(
        const std::shared_ptr<tooey::AstNode>& root,
        const std::string& target_id) = 0;

    virtual bool update_properties(
        const std::shared_ptr<tooey::AstNode>& root,
        const std::string& target_id,
        const std::vector<std::pair<std::string, std::string>>& properties) = 0;
};

} // namespace editor::services
```

#### 4.2.3 `IAstQuery.hpp`

```cpp
// src/services/IAstQuery.hpp
#pragma once
#include "tooey/ast.hpp"
#include <string>
#include <memory>

namespace editor::services {

class IAstQuery {
public:
    virtual ~IAstQuery() = default;

    virtual std::shared_ptr<tooey::AstNode> find_by_id(
        const std::shared_ptr<tooey::AstNode>& root,
        const std::string& id) const = 0;
};

} // namespace editor::services
```

#### 4.2.4 `IDslCompilerService.hpp`

```cpp
// src/services/IDslCompilerService.hpp
#pragma once
#include "tooey/ast.hpp"
#include <string>
#include <memory>
#include <vector>
#include <cstdint>

namespace editor::services {

struct LintDiagnostic {
    int      line;
    int      start_col;
    int      end_col;
    uint32_t color_rgba; // packed RGBA for interface cleanliness
};

class IDslCompilerService {
public:
    virtual ~IDslCompilerService() = default;

    // Tokenizes + parses DSL. Returns nullptr on hard parse failure.
    virtual std::shared_ptr<tooey::AstNode> compile(const std::string& dsl) = 0;

    // Runs the linter only (no AST produced). Returns diagnostics.
    virtual std::vector<LintDiagnostic> lint(const std::string& dsl) = 0;
};

} // namespace editor::services
```

**Why interfaces matter**: With `IDslCompilerService` injected into `EditorViewModel` and `EditorController`, a unit test can inject `MockDslCompilerService` that returns a pre-built `AstNode` tree without ever running the real lexer. This allows testing `selectElement()`, `updateProperty()`, `undo()`/`redo()` entirely in memory.

---

#### 4.2.5 `HistoryManager.hpp/.cpp` — Undo/Redo Extracted

**Problem**: Undo/redo is implemented with `std::vector<std::string>` fields and `record_history()` buried inside `EditorViewModel`. The logic is interleaved with parsing calls (`updateCanvasFromDsl`). The class has 4 private members and a private method dedicated solely to history — nearly a separate class already hiding inside the ViewModel.

**Fix**: A dedicated `HistoryManager`:

```cpp
// src/services/HistoryManager.hpp
#pragma once
#include <string>
#include <vector>
#include <cstddef>
#include <chrono>

namespace editor::services {

class HistoryManager {
public:
    explicit HistoryManager(std::size_t max_depth = 100);

    // Records a DSL snapshot. Respects a 1-second debounce to avoid recording
    // every single keystroke as a separate undo state.
    void record(const std::string& dsl);

    // Pops the previous state and pushes current_dsl onto redo stack.
    // Returns empty string if undo stack is empty.
    std::string undo(const std::string& current_dsl);

    // Pops the next state and pushes current_dsl onto undo stack.
    // Returns empty string if redo stack is empty.
    std::string redo(const std::string& current_dsl);

    bool can_undo() const noexcept;
    bool can_redo() const noexcept;

    void clear();

private:
    std::vector<std::string>                        undo_stack_;
    std::vector<std::string>                        redo_stack_;
    std::size_t                                     max_depth_;
    std::string                                     last_recorded_;
    std::chrono::steady_clock::time_point           last_record_time_;
};

} // namespace editor::services
```

The `HistoryManager` is **fully unit-testable** without any UI framework:

```cpp
// In a unit test:
editor::services::HistoryManager history;
history.record("VBox id=root:\n");
history.record("VBox id=root:\n    Label text=\"hello\"\n");
auto prev = history.undo("VBox id=root:\n    Label text=\"hello\"\n");
assert(prev == "VBox id=root:\n");
assert(history.can_redo());
auto next = history.redo(prev);
assert(next == "VBox id=root:\n    Label text=\"hello\"\n");
```

---

### 4.3 `controllers/EditorController.hpp/.cpp`

**Problem**: All View ↔ ViewModel event bindings — 5 button `on_click` handlers, 3 list `on_selected_changed` handlers, 1 canvas pointer handler, 3 code editor event handlers, and 5 reactive subscriptions — are inline lambdas in `main()`. This is ~200 lines of event wiring with zero testability, all in the application entry point.

**Fix**: Extract into an `EditorController` class that owns the subscriptions and wiring:

```cpp
// src/controllers/EditorController.hpp
#pragma once
#include "EditorView.hpp"
#include "viewmodels/EditorViewModel.hpp"
#include "SelectionOverlay.hpp"
#include "gooey/mvvmc/scoped_subscription.hpp"
#include "gooey/mvvmc/theme.hpp"
#include <memory>
#include <vector>

namespace editor {

// Mediator between EditorView and EditorViewModel.
// Owns all reactive subscriptions for the editor session.
class EditorController {
public:
    EditorController(
        std::shared_ptr<EditorView>           view,
        std::shared_ptr<EditorViewModel>      view_model,
        std::shared_ptr<SelectionOverlay>     overlay,
        std::shared_ptr<gooey::ThemeManager>  theme_manager);

    // Call once to wire up all event handlers and subscriptions.
    void connect();

private:
    void wire_toolbox_buttons();
    void wire_hierarchy_list();
    void wire_canvas_pointer();
    void wire_code_editor();
    void wire_dsl_preview_subscription();
    void sync_initial_preview();

    std::shared_ptr<EditorView>           view_;
    std::shared_ptr<EditorViewModel>      view_model_;
    std::shared_ptr<SelectionOverlay>     overlay_;
    std::shared_ptr<gooey::ThemeManager>  theme_manager_;

    // Owns the lifetime of all reactive subscriptions.
    std::vector<gooey::mvvmc::ScopedSubscription> subs_;

    // Stores the current live preview root element.
    std::shared_ptr<gooey::mvvmc::GooeyElement> active_preview_;
};

} // namespace editor
```

**Result in `main.cpp`**:

```cpp
auto controller = std::make_shared<editor::EditorController>(
    editor_view, view_model, overlay, theme_manager);
controller->connect();
```

**Testing benefit**: `EditorController` can be constructed with a fake/stub `EditorView` and `EditorViewModel` in a test, and `connect()` can be called to verify that all expected event handlers are attached without starting a real display server.

---

### 4.4 `menus/EditorMenuBuilder.hpp/.cpp`

**Problem**: `build_editor_menu_categories()` is a free function in an anonymous namespace in `main.cpp`. It is tightly coupled to `EditorViewModel`, `ThemeManager`, and `LocalizationManager` simultaneously, takes them all as parameters, and is called both from `main()` and from two separate subscription lambdas also in `main()`.

**Fix**: A proper builder class with reactive auto-rebuild:

```cpp
// src/menus/EditorMenuBuilder.hpp
#pragma once
#include "gooey/controls/menubar.hpp"
#include "gooey/mvvmc/theme.hpp"
#include "viewmodels/EditorViewModel.hpp"
#include "gooey/mvvmc/scoped_subscription.hpp"
#include <memory>
#include <vector>

namespace editor::menus {

// Builds and maintains the editor's menu categories.
// Subscribes to theme/locale changes and rebuilds automatically.
class EditorMenuBuilder {
public:
    EditorMenuBuilder(
        std::shared_ptr<EditorViewModel>          view_model,
        std::shared_ptr<gooey::ThemeManager>      theme_manager,
        std::shared_ptr<gooey::controls::MenuBar> menu_bar);

    // Performs initial build and wires reactive rebuild subscriptions.
    void initialize();

private:
    std::vector<gooey::controls::MenuCategory> build() const;

    gooey::controls::MenuCategory build_file_menu() const;
    gooey::controls::MenuCategory build_edit_menu() const;
    gooey::controls::MenuCategory build_view_menu() const;
    gooey::controls::MenuCategory build_help_menu() const;

    std::shared_ptr<EditorViewModel>          view_model_;
    std::shared_ptr<gooey::ThemeManager>      theme_manager_;
    std::shared_ptr<gooey::controls::MenuBar> menu_bar_;

    // Owns reactive subscriptions for auto-rebuild on theme/locale change.
    std::vector<gooey::mvvmc::ScopedSubscription> subs_;
};

} // namespace editor::menus
```

**Benefits**:
- Each menu category has its own private builder method (SRP per menu)
- The reactive rebuild subscription is encapsulated — not scattered across `main()`
- Can be tested by constructing with mock ViewModels and asserting on returned category structures
- Adding a new menu (e.g., "Tools") requires only adding `build_tools_menu()` and updating `build()` — no changes to `main.cpp` (OCP)

---

### 4.5 `views/PropertyCell.hpp/.cpp`

**Problem**: `PropertyCell` lives in an anonymous namespace in `main.cpp`. It directly references `gooey::Application::get_instance()` (a global singleton) and has hardcoded colors that don't respond to theme changes.

**Fix 1 — Extract to its own files**:

```cpp
// src/views/PropertyCell.hpp
#pragma once
#include "gooey/mvvmc/gooey_element.hpp"
#include "gooey/mvvmc/i_interactive.hpp"
#include "gooey/mvvmc/theme.hpp"
#include "gooey/controls/text_box.hpp"
#include "viewmodels/EditorViewModel.hpp"
#include <memory>
#include <functional>
#include <string>

namespace editor::views {

class PropertyCell
    : public gooey::GooeyElement
    , public gooey::mvvmc::IInteractive
    , public std::enable_shared_from_this<PropertyCell>
{
public:
    // Inject a focus callback so the cell never touches Application::get_instance()
    using FocusProvider = std::function<void(std::shared_ptr<gooey::mvvmc::IInteractive>)>;

    PropertyCell(
        int                          row_index,
        std::weak_ptr<EditorViewModel> weak_vm,
        std::function<void()>          on_clear_selection,
        FocusProvider                  focus_provider);

    // Theme integration: called when active theme changes
    void apply_style(const gooey::mvvmc::Style& style) override;
    void set_theme_manager(std::shared_ptr<gooey::mvvmc::ThemeManager> manager) override;

    // ... rest of interface unchanged
};

} // namespace editor::views
```

**Fix 2 — Inject the focus provider** (eliminates global singleton call):

Instead of `gooey::Application::get_instance()->get_controller()`, the `EditorController` passes a lambda:

```cpp
// In EditorController::wire_property_grid():
auto focus_provider = [weak_ctrl = std::weak_ptr<gooey::mvvmc::IController>(controller_ptr)](
    std::shared_ptr<gooey::mvvmc::IInteractive> elem) {
    if (auto ctrl = weak_ctrl.lock()) {
        ctrl->set_focused_element(elem);
    }
};
```

**Fix 3 — Theme-aware drawing**: `apply_style()` stores the themed text color and selection color, replacing hardcoded `Color{240, 240, 240}` and `Color{0, 120, 215, 30}`. In the `draw()` method, use the stored colors from `apply_style()` instead of literals.

---

### 4.6 `views/PropertyGridConfigurator.hpp/.cpp`

**Problem**: DataGrid construction and column factory/binder lambdas are 70+ lines scattered through `main()`. They capture `viewModel` and create `PropertyCell` instances — this logic currently lives outside the class that should own it.

**Fix**: A configurator class:

```cpp
// src/views/PropertyGridConfigurator.hpp
#pragma once
#include "gooey/controls/datagrid.hpp"
#include "gooey/mvvmc/scoped_subscription.hpp"
#include "viewmodels/EditorViewModel.hpp"
#include <memory>
#include <vector>

namespace editor::views {

class PropertyGridConfigurator {
public:
    // Configures columns on a new DataGrid and wires propertyItems subscription.
    // Out parameter: subs receives the subscription that keeps grid in sync.
    static std::shared_ptr<gooey::controls::DataGrid> create_and_configure(
        std::shared_ptr<EditorViewModel>               view_model,
        std::vector<gooey::mvvmc::ScopedSubscription>& out_subs,
        PropertyCell::FocusProvider                    focus_provider);

private:
    static gooey::controls::DataGridColumn make_property_name_column();
    static gooey::controls::DataGridColumn make_property_value_column(
        std::shared_ptr<EditorViewModel> view_model,
        PropertyCell::FocusProvider      focus_provider);
};

} // namespace editor::views
```

---

### 4.7 `theme/ThemeRegistry.hpp/.cpp`

**Problem**: 35 lines of `ThemeManager` configuration are in `main()`. Adding a new theme requires editing `main.cpp`.

**Fix**: A registry that encapsulates all theme construction:

```cpp
// src/theme/ThemeRegistry.hpp
#pragma once
#include "gooey/mvvmc/theme.hpp"
#include <memory>

namespace editor::theme {

// Creates and registers all built-in themes into a new ThemeManager.
class ThemeRegistry {
public:
    static std::shared_ptr<gooey::ThemeManager> create_default_manager();

private:
    static std::shared_ptr<gooey::Theme> make_dark_theme();
    static std::shared_ptr<gooey::Theme> make_light_theme();
    static std::shared_ptr<gooey::Theme> make_lofi_theme();
    static std::shared_ptr<gooey::Theme> make_cyberpunk_theme();
};

} // namespace editor::theme
```

**Adding a new theme** now only requires adding a `make_synthwave_theme()` method in `ThemeRegistry.cpp` and registering it in `create_default_manager()` — **zero changes to `main.cpp`**. This is the Open/Closed Principle in practice.

---

### 4.8 `i18n/TranslationLoader.hpp/.cpp`

**Problem**: Four `std::map<string,string>` dictionaries totalling ~130 translation entries are embedded directly in `main()` (lines 426–553). Adding a new UI key requires editing `main.cpp` in 4 places. Adding a new language requires editing `main.cpp` again.

**Fix**: A `TranslationLoader` that owns and loads all dictionaries:

```cpp
// src/i18n/TranslationLoader.hpp
#pragma once
#include <string>
#include <vector>

namespace editor::i18n {

class TranslationLoader {
public:
    // Loads all built-in locale dictionaries into LocalizationManager.
    // Call once at application startup before any UI is shown.
    static void load_all();

    // Returns all locale codes supported by this build.
    static std::vector<std::string> supported_locales();

private:
    static void load_en_US();
    static void load_es_ES();
    static void load_de_DE();
    static void load_fr_FR();
};

} // namespace editor::i18n
```

**Future extensibility**: `load_all()` can be extended to scan a `locales/` directory for JSON or `.ini` files, implementing external locale support without changing `main.cpp` at all. Adding French-Canadian would be `load_fr_CA()` — no callers need to change.

---

### 4.9 `app/AppBootstrap.hpp/.cpp`

**Problem**: Window creation and `gooey::Application` configuration are inline in `main()` alongside business logic.

**Fix**: Isolate bootstrap:

```cpp
// src/app/AppBootstrap.hpp
#pragma once
#include "gooey/application.hpp"
#include <string>

namespace editor::app {

struct AppConfig {
    int         width  = 1024;
    int         height = 768;
    std::string title  = "OOEY-GOOEY Layout Builder";
};

// Creates the window and initializes gooey::Application.
// Returns false and prints to stderr if window creation fails.
bool bootstrap(gooey::Application& app, const AppConfig& config);

} // namespace editor::app
```

---

### 4.10 Slim `EditorViewModel` (After Refactoring)

The ViewModel after refactoring should **only**:
1. Hold reactive `Property<T>` fields (the observable data model)
2. Expose command methods that delegate entirely to injected services
3. NOT contain undo/redo stack logic
4. NOT contain parsing/compilation logic
5. NOT contain serialization logic

```cpp
// src/viewmodels/EditorViewModel.hpp (target state)
class EditorViewModel : public gooey::mvvmc::ViewModel {
public:
    explicit EditorViewModel(
        std::shared_ptr<services::IAstMutator>    mutator,
        std::shared_ptr<services::IAstSerializer> serializer,
        std::shared_ptr<services::IAstQuery>      query,
        std::shared_ptr<services::IDslCompilerService> compiler,
        std::shared_ptr<services::HistoryManager> history);

    // Observable reactive properties (snake_case per coding standards)
    Property<std::vector<domain::ToolboxItem>>   toolbox_items;
    Property<std::vector<domain::HierarchyItem>> hierarchy_items;
    Property<std::vector<domain::PropertyItem>>  property_items;
    Property<std::string>                        dsl_text;

    int  selected_index{-1};

    // Commands
    void select_element(int index);
    void update_property(int index, const std::string& new_value);
    void add_control_to_canvas(const std::string& type);
    void update_canvas_from_dsl(const std::string& dsl);
    void delete_selected_element();
    void move_selected_element_up();
    void move_selected_element_down();
    void undo();
    void redo();

    std::shared_ptr<tooey::AstNode> current_ast;

private:
    // Injected services — all accessed via pure virtual interfaces
    std::shared_ptr<services::IAstMutator>        mutator_;
    std::shared_ptr<services::IAstSerializer>     serializer_;
    std::shared_ptr<services::IAstQuery>          query_;
    std::shared_ptr<services::IDslCompilerService> compiler_;
    std::shared_ptr<services::HistoryManager>     history_;

    // Private — no longer exposed to external callers
    bool is_updating_{false};

    void rebuild_hierarchy_items(const std::shared_ptr<tooey::AstNode>& node, int indent);
    void update_property_items();
    void apply_ast_change(const std::string& new_dsl);
};
```

**Key difference from current**: `is_updating_` is now private. `EditorController` no longer needs to read or set it. This restores proper encapsulation.

---

### 4.11 Slim `main.cpp` (Composition Root Only)

After all phases, `main.cpp` becomes:

```cpp
#include "app/AppBootstrap.hpp"
#include "theme/ThemeRegistry.hpp"
#include "i18n/TranslationLoader.hpp"
#include "services/AstSerializer.hpp"
#include "services/AstMutator.hpp"
#include "services/AstQuery.hpp"
#include "services/DslCompilerService.hpp"
#include "services/HistoryManager.hpp"
#include "viewmodels/EditorViewModel.hpp"
#include "controllers/EditorController.hpp"
#include "menus/EditorMenuBuilder.hpp"
#include "views/PropertyGridConfigurator.hpp"
#include "SelectionOverlay.hpp"
#include "EditorView.hpp"
#include "gooey/application.hpp"

extern "C" const char* __lsan_default_suppressions() {
    return "leak:libGLX_mesa.so\nleak:libGLX.so\nleak:libGL.so\n";
}

int main() {
    gooey::Application app;
    if (!editor::app::bootstrap(app, {})) return 1;

    // Infrastructure
    auto theme_manager = editor::theme::ThemeRegistry::create_default_manager();
    editor::i18n::TranslationLoader::load_all();
    app.set_theme_manager(theme_manager);

    // Service layer
    auto serializer = std::make_shared<editor::services::AstSerializer>();
    auto mutator    = std::make_shared<editor::services::AstMutator>();
    auto query      = std::make_shared<editor::services::AstQuery>();
    auto compiler   = std::make_shared<editor::services::DslCompilerService>();
    auto history    = std::make_shared<editor::services::HistoryManager>();

    // ViewModel
    auto view_model = std::make_shared<EditorViewModel>(
        mutator, serializer, query, compiler, history);

    // View
    auto editor_view = std::make_shared<EditorView>(view_model);
    auto overlay     = std::make_shared<editor::SelectionOverlay>();
    editor_view->set_theme_manager(theme_manager);

    // Controller (wires View ↔ ViewModel)
    auto controller = std::make_shared<editor::EditorController>(
        editor_view, view_model, overlay, theme_manager);
    controller->connect();

    // Menu system
    auto menu_builder = std::make_shared<editor::menus::EditorMenuBuilder>(
        view_model, theme_manager, editor_view->menuBar);
    menu_builder->initialize();

    // Property inspector grid
    auto prop_grid = editor::views::PropertyGridConfigurator::create_and_configure(
        view_model, controller->subscriptions(), controller->focus_provider());
    editor_view->rightSidebar->add_child(prop_grid);

    app.set_root_view(editor_view);

    const char* auto_quit = std::getenv("OOEY_AUTO_QUIT");
    if (auto_quit) {
        app.dispatch([&app]() { app.quit(); });
    }

    app.run();
    return 0;
}
```

**Result**: ~65 lines. Every line is wiring. Zero business logic.

---

## 5. Design Patterns to Apply

### 5.1 Builder Pattern — `EditorMenuBuilder`

Constructs complex `MenuCategory` aggregates step-by-step via private builder methods per menu. The builder encapsulates the assembly logic and owns the reactive update lifecycle.

**Applies to**: `menus/EditorMenuBuilder.hpp/.cpp`

**Intent**: Separate construction of a complex object (the menu category vector) from its representation (the `MenuBar` it populates). Allows different menus to be added or modified independently without affecting the builder's callers.

---

### 5.2 Strategy Pattern — `IAstSerializer`, `IAstMutator`

Defines a family of interchangeable algorithms behind a common interface. The concrete implementation (`AstSerializer`) can be substituted with a `JsonAstExporter` or `XmlAstSerializer` without changing any caller.

**Applies to**: `services/IAstSerializer.hpp`, `services/IAstMutator.hpp`

**Real benefit**: Unit tests can use `MockAstSerializer` that always returns `"MOCK_DSL"` — no real parsing needed to test ViewModel logic.

---

### 5.3 Facade Pattern — `DslCompilerService`

Provides a simplified, unified interface to the three-component subsystem of `tooey::Lexer`, `tooey::Parser`, and `tooey::Linter`. Callers (previously `main.cpp` and `EditorViewModel`) now call `IDslCompilerService::compile()` and `IDslCompilerService::lint()` — they don't know or care that three separate systems are involved.

**Applies to**: `services/IDslCompilerService.hpp`, `services/DslCompilerService.hpp/.cpp`

---

### 5.4 Observer / Reactive Pattern — Centralized Subscriptions

The `EditorController` owns all `ScopedSubscription` objects in its `subs_` vector, ensuring proper destruction when the controller is destroyed. Previously, subscriptions were scattered across `main()` locals, making lifetime management implicit and accident-prone.

**Applies to**: `controllers/EditorController.hpp/.cpp`

**Invariant enforced**: When `EditorController` is destroyed, all subscriptions in `subs_` are automatically cancelled via `ScopedSubscription` RAII destructors — no manual cleanup required.

---

### 5.5 Registry Pattern — `ThemeRegistry`

A factory that constructs and registers all known named themes. New themes are added by adding entries to the registry's factory methods — the set of registered themes is open for extension but callers are closed to modification.

**Applies to**: `theme/ThemeRegistry.hpp/.cpp`

---

### 5.6 Composition Root + Constructor Injection

The slim `main.cpp` acts as the single **composition root**: the one place where all concrete implementations are instantiated and injected into each other. This is not a "framework" for DI — it is manual DI applied at the application boundary. Everything inside the composition root depends only on abstractions (`IAstMutator`, `ICanvasPreviewService`, etc.).

**Why constructor injection over setter injection**: Constructor injection makes dependencies explicit and required — you cannot construct an `EditorViewModel` without providing a `HistoryManager`. Setter injection allows objects to be partially constructed in invalid states.

---

### 5.7 Mediator Pattern — `EditorController`

`EditorController` acts as a mediator between `EditorView` and `EditorViewModel`. Rather than the View and ViewModel having direct references to each other's internals, the Controller brokers all communication. This reduces coupling and makes the interaction protocol explicit in one place.

---

## 6. SOLID Violations and Fixes

### S — Single Responsibility Principle

| Violation | Current Location | Fix | Why It Matters |
|---|---|---|---|
| `PropertyCell` widget in bootstrap file | `main.cpp` anon namespace | Extract to `views/PropertyCell.hpp/.cpp` | Widget is untestable and unreusable |
| Menu building in bootstrap | `main.cpp` free function | Extract to `menus/EditorMenuBuilder` | Menus can evolve independently of bootstrap |
| Theme creation in bootstrap | `main.cpp` inline code | Extract to `theme/ThemeRegistry` | Adding themes requires no main.cpp change |
| Translation loading in bootstrap | `main.cpp` inline maps | Extract to `i18n/TranslationLoader` | Adding languages requires no main.cpp change |
| DataGrid wiring in bootstrap | `main.cpp` inline code | Extract to `views/PropertyGridConfigurator` | Grid logic encapsulated, independently changeable |
| Event binding in bootstrap | `main.cpp` lambdas | Extract to `controllers/EditorController` | Controller is independently testable |
| Undo/redo stack in ViewModel | `EditorViewModel.cpp` fields | Extract to `services/HistoryManager` | History fully unit-testable without UI |
| Serialization in `AstService` | `AstService.cpp` static | Split to `AstSerializer` | Single concern, mockable in tests |
| Mutation in `AstService` | `AstService.cpp` static | Split to `AstMutator` | Single concern, mockable in tests |
| Query in `AstService` | `AstService.cpp` static | Split to `AstQuery` | Single concern, mockable in tests |

### O — Open/Closed Principle

| Violation | Fix |
|---|---|
| Adding a theme requires editing `main.cpp` | `ThemeRegistry::make_synthwave_theme()`, no main changes |
| Adding a locale requires editing `main.cpp` | `TranslationLoader::load_fr_CA()`, no main changes |
| Adding a new menu requires editing anonymous free function | `EditorMenuBuilder::build_tools_menu()`, no caller changes |
| Adding a new control type to toolbox requires `if-else` chain edit | Already uses `TypeRegistry` — ensure `add_control_to_canvas` fully delegates |

### L — Liskov Substitution Principle

| Issue | Fix |
|---|---|
| `AstService` is fully static — impossible to substitute with a mock | Concrete classes implement `IAstSerializer`, `IAstMutator`, `IAstQuery` |
| `PropertyCell` directly calls `Application::get_instance()` | Inject `FocusProvider` — any callable with the right signature works |
| `DynamicInterpreter::interpret()` is static | Wrap behind `ICanvasPreviewService` (future improvement) |

### I — Interface Segregation Principle

| Issue | Fix |
|---|---|
| `AstService` is a monolith with 6 unrelated methods, all static | Three focused interfaces: `IAstSerializer`, `IAstMutator`, `IAstQuery` — callers only depend on the one they use |
| `EditorViewModel` exposes `is_updating_` as a public bool | Make private; `EditorController` should not need to observe ViewModel internal sync flags |

### D — Dependency Inversion Principle

| Violation | Fix |
|---|---|
| `EditorViewModel` directly calls `tooey::Lexer`, `tooey::Parser` | Inject `IDslCompilerService` — ViewModel no longer knows about tooey |
| `EditorViewModel` directly calls `AstService` static methods | Inject `IAstMutator`, `IAstSerializer`, `IAstQuery` |
| `PropertyCell` calls `gooey::Application::get_instance()` | Inject `FocusProvider` lambda — no singleton dependency |
| `main.cpp` directly builds `ThemeManager` with raw style values | `ThemeRegistry` abstracts theme construction |
| `EditorViewModel` builds `HistoryManager` logic internally | Inject `HistoryManager` — ViewModel depends on its abstraction |

---

## 7. File-by-File Refactoring Plan

### Phase 1: Extract Domain Models (zero behaviour changes)

**Goal**: Zero-risk extraction of plain data types to break include coupling.

1. **Create** `src/domain/models.hpp` with `ToolboxItem`, `HierarchyItem`, `PropertyItem` in `editor::domain` namespace.
2. **Update** `EditorViewModel.hpp` to `#include "domain/models.hpp"` and remove the struct definitions from that file.
3. **Update** all files referencing these structs — currently only `main.cpp` and `EditorViewModel.cpp`.
4. **Build** — must compile clean with zero changes to observable behaviour.
5. **Commit** as `refactor: extract domain model types to domain/models.hpp`.

**Estimated effort**: 30–60 minutes. **Risk**: Negligible.

---

### Phase 2: Extract `HistoryManager`

**Goal**: Remove undo/redo from `EditorViewModel` without changing observable behaviour.

1. **Create** `src/services/HistoryManager.hpp/.cpp` as described in §4.2.5.
2. **Modify** `EditorViewModel`:
   - Remove: `undo_stack_`, `redo_stack_`, `last_dsl_`, `last_typing_record_time_`, `record_history()`.
   - Add: `std::shared_ptr<services::HistoryManager> history_` private member.
   - Update constructor to accept and store `HistoryManager`.
   - Delegate `undo()`, `redo()`, and all `record_history()` call sites to `history_`.
3. **Update** `main.cpp` to construct `HistoryManager` and pass it to `EditorViewModel`.
4. **Update** `CMakeLists.txt` to add `src/services/HistoryManager.cpp`.
5. **Write** `tests/test_history_manager.cpp` with the test cases in §8.
6. **Build** and run the editor to smoke-test.
7. **Commit** as `refactor: extract HistoryManager from EditorViewModel`.

**Estimated effort**: 2–3 hours. **Risk**: Low — functional behaviour identical.

---

### Phase 3: Extract Service Interfaces + Concrete Implementations

**Goal**: Make AST operations injectable and mockable.

1. **Create** `src/services/IAstSerializer.hpp`, `IAstMutator.hpp`, `IAstQuery.hpp`.
2. **Create** concrete implementations:
   - `AstSerializer.cpp` — copies logic from `AstService::serialize_ast` verbatim.
   - `AstMutator.cpp` — copies logic from `add_child_node`, `delete_node`, `move_node_up`, `move_node_down`, `update_node_properties`.
   - `AstQuery.cpp` — copies logic from `find_node_by_id`.
3. **Keep** `AstService.hpp/.cpp` as a **thin deprecated facade** that delegates to the new classes — avoids a big-bang refactor that risks regressions.
4. **Inject** interfaces into `EditorViewModel` constructor.
5. **Update** `CMakeLists.txt` to add new `.cpp` files.
6. **Build** and smoke-test.
7. **Commit** as `refactor: extract IAstSerializer, IAstMutator, IAstQuery service interfaces`.

**Estimated effort**: 3–4 hours. **Risk**: Medium — touches `EditorViewModel` constructor.

---

### Phase 4: Extract `PropertyCell`

**Goal**: Move `PropertyCell` to a testable, themed, dependency-injected widget.

1. **Create** `src/views/PropertyCell.hpp/.cpp` copying all logic from the anonymous namespace.
2. **Remove** `PropertyCell` from the anonymous namespace in `main.cpp`.
3. **Add** `FocusProvider` injection type alias and constructor parameter.
4. **Replace** `gooey::Application::get_instance()->get_controller()` with `focus_provider_(shared_from_this())`.
5. **Implement** `apply_style()` to store theme colors and use them in `draw()`.
6. **Update** `CMakeLists.txt` to add `src/views/PropertyCell.cpp`.
7. **Update** `main.cpp` to include `views/PropertyCell.hpp` and pass a focus provider lambda.
8. **Build** and smoke-test.
9. **Commit** as `refactor: extract PropertyCell to views/PropertyCell with theme support`.

**Estimated effort**: 2 hours. **Risk**: Low — the code is moved verbatim; the focus provider injection is additive.

---

### Phase 5: Extract `PropertyGridConfigurator`

**Goal**: Move DataGrid wiring out of `main.cpp`.

1. **Create** `src/views/PropertyGridConfigurator.hpp/.cpp`.
2. **Move** column factory and binder lambda definitions.
3. **Move** the `subs.push_back(viewModel->propertyItems.subscribe(...))` call.
4. **Update** `CMakeLists.txt` to add `src/views/PropertyGridConfigurator.cpp`.
5. **Update** `main.cpp` to call `PropertyGridConfigurator::create_and_configure(...)`.
6. **Build** and smoke-test.
7. **Commit** as `refactor: extract PropertyGridConfigurator from main`.

**Estimated effort**: 1–2 hours. **Risk**: Low.

---

### Phase 6: Extract `EditorController`

**Goal**: Move all event wiring out of `main()`.

1. **Create** `src/controllers/EditorController.hpp/.cpp`.
2. **Move** each `editorView->btn*->on_click = ...` into `wire_toolbox_buttons()`.
3. **Move** `editorView->hierarchyList->on_selected_changed = ...` into `wire_hierarchy_list()`.
4. **Move** `editorView->previewCanvas->on_canvas_pointer = ...` into `wire_canvas_pointer()`.
5. **Move** code editor event assignments into `wire_code_editor()`.
6. **Move** `subs.push_back(viewModel->dslText.subscribe(...))` and `subs.push_back(viewModel->hierarchyItems.subscribe(...))` into `wire_dsl_preview_subscription()`.
7. **Move** the initial DSL preview sync into `sync_initial_preview()`.
8. **Add** `connect()` method that calls all `wire_*()` methods.
9. **Update** `CMakeLists.txt`.
10. **Update** `main.cpp` to construct and `connect()` the controller.
11. **Build** and smoke-test all interaction paths.
12. **Commit** as `refactor: extract EditorController from main`.

**Estimated effort**: 3–4 hours. **Risk**: Medium-high — most complex phase. Thoroughly smoke-test all buttons, list clicks, canvas interactions.

---

### Phase 7: Extract `EditorMenuBuilder`

**Goal**: Move menu construction and reactive rebuild out of `main()`.

1. **Create** `src/menus/EditorMenuBuilder.hpp/.cpp`.
2. **Move** the logic of `build_editor_menu_categories()` into `build()`.
3. **Split** into `build_file_menu()`, `build_edit_menu()`, `build_view_menu()`, `build_help_menu()` private methods.
4. **Move** the two `subs.push_back(theme_manager->active_theme.subscribe(...))` and `subs.push_back(LocalizationManager::get().active_locale.subscribe(...))` into `initialize()`.
5. **Update** `CMakeLists.txt`.
6. **Update** `main.cpp` to construct and call `menu_builder->initialize()`.
7. **Remove** the now-unused `build_editor_menu_categories` free function from `main.cpp`.
8. **Build** and smoke-test all menus and locale/theme switching.
9. **Commit** as `refactor: extract EditorMenuBuilder from main`.

**Estimated effort**: 2 hours. **Risk**: Low.

---

### Phase 8: Extract `ThemeRegistry` and `TranslationLoader`

**Goal**: Isolate infrastructure setup from `main()`.

1. **Create** `src/theme/ThemeRegistry.hpp/.cpp` with `make_dark_theme()`, `make_light_theme()`, `make_lofi_theme()`, `make_cyberpunk_theme()`, `create_default_manager()`.
2. **Create** `src/i18n/TranslationLoader.hpp/.cpp` with `load_en_US()`, `load_es_ES()`, `load_de_DE()`, `load_fr_FR()`, `load_all()`.
3. **Update** `CMakeLists.txt`.
4. **Update** `main.cpp` to call `ThemeRegistry::create_default_manager()` and `TranslationLoader::load_all()` — removing the inline theme and dict construction.
5. **Build** and smoke-test theme switching and language switching.
6. **Write** `tests/test_theme_registry.cpp` and `tests/test_translation_loader.cpp`.
7. **Commit** as `refactor: extract ThemeRegistry and TranslationLoader infrastructure`.

**Estimated effort**: 2 hours. **Risk**: Low.

---

### Phase 9: Slim `main.cpp` and Final Cleanup

**Goal**: `main.cpp` becomes a pure composition root of ~60 lines.

1. After all phases above, `main.cpp` should contain only the pattern shown in §4.11.
2. Remove any remaining inline business logic.
3. Remove all `#include` headers that are no longer needed at the `main.cpp` level.
4. Run `cmake --build build` — must succeed with zero errors.
5. Run the editor under ASan: `./build/ooey_gooey_editor` — must be clean.
6. Run `OOEY_AUTO_QUIT=1 ./build/ooey_gooey_editor` — verify no LSan leaks.
7. Commit as `refactor: slim main.cpp to composition root only`.

**Estimated effort**: 1 hour. **Risk**: Low — mostly removal.

---

## 8. New Test Targets

After refactoring, the following unit tests become possible **without a display server or GUI framework**:

### `tests/test_history_manager.cpp`

```
✓ record() adds a state to the undo stack
✓ undo() returns the previous state
✓ undo() pushes current onto redo stack
✓ redo() returns the next state from redo stack
✓ redo() pushes current onto undo stack
✓ undo() on empty stack returns empty string
✓ redo() on empty stack returns empty string
✓ Recording the same state twice produces only one stack entry (deduplication)
✓ Stack is clamped to max_depth (oldest entries evicted)
✓ clear() empties both stacks
✓ can_undo() returns false on empty undo stack
✓ can_redo() returns false on empty redo stack
```

### `tests/test_ast_serializer.cpp`

```
✓ Null AST returns empty string
✓ Single-node AST serializes with correct node type
✓ Nested nodes serialize with 4-space indent per level
✓ String properties are wrapped in quotes
✓ Localization properties produce @tr("...") syntax
✓ Binding properties produce @binding.xxx syntax
✓ Nodes with children use colon-newline separator
✓ Nodes without children serialize on a single line
✓ Properties in inline_props appear inline on the node line
✓ Non-inline properties appear on subsequent indented lines
```

### `tests/test_ast_mutator.cpp`

```
✓ add_child to root by ID succeeds
✓ add_child to non-existent parent ID returns false
✓ delete_node removes node from its parent's children list
✓ delete_node on non-existent ID returns false
✓ move_node_up swaps node with its previous sibling
✓ move_node_up on first child returns false (already at top)
✓ move_node_down swaps node with its next sibling
✓ move_node_down on last child returns false
✓ update_properties modifies only the target node's properties
✓ update_properties leaves sibling nodes unchanged
✓ update_properties on non-existent ID returns false
```

### `tests/test_ast_query.cpp`

```
✓ find_by_id on root by its own ID returns root
✓ find_by_id on a deeply nested node returns the correct node
✓ find_by_id on non-existent ID returns nullptr
✓ find_by_id on null root returns nullptr
```

### `tests/test_translation_loader.cpp`

```
✓ After load_all(), en_US locale has "menu_file" == "File"
✓ After load_all(), es_ES locale has "menu_file" == "Archivo"
✓ After load_all(), de_DE locale has "delete" == "Löschen"
✓ After load_all(), fr_FR locale has "menu_exit" == "Quitter"
✓ After load_all(), supported_locales() returns all 4 locale codes
✓ All 4 locales have the same set of keys (completeness check)
```

### `tests/test_theme_registry.cpp`

```
✓ create_default_manager() returns a non-null ThemeManager
✓ Created manager has "dark", "light", "lofi", "cyberpunk" themes registered
✓ Dark theme's "window" style has fill_color == Color{22, 22, 28}
✓ Light theme's "window" style has fill_color == Color{240, 240, 245}
✓ Cyberpunk theme's "menubar" has text_color == Color{0, 255, 255}
✓ After set_active_theme("light"), active_theme->name == "light"
```

---

## 9. Migration Strategy

The refactoring must be done **incrementally** without ever breaking the working build. The recommended order maintains a compileable-and-runnable state after each phase:

```
Phase 1           Phase 2           Phase 3
Extract models  → Extract History → Extract Services
    ↓                 ↓                 ↓
Phase 4           Phase 5           Phase 6
Extract Cell    → Extract Grid    → Extract Controller
    ↓                 ↓                 ↓
Phase 7           Phase 8           Phase 9
Extract Menus   → Extract Infra   → Slim main.cpp
```

**Rules for each phase**:
1. Create new files
2. Move existing code verbatim (or near-verbatim) — do **not** redesign while moving
3. Update include paths and `CMakeLists.txt`
4. Run `cmake --build build` — must succeed with zero errors and zero new warnings
5. Run `./build/ooey_gooey_editor` — smoke-test key user flows
6. Run `OOEY_AUTO_QUIT=1 ./build/ooey_gooey_editor` to check for ASan/LSan errors
7. Commit with a clear, phase-labelled message

> [!IMPORTANT]
> **Never combine multiple phases in one commit.** If a regression is introduced, a single-phase commit makes the diff and the cause immediately obvious.

> [!TIP]
> The most valuable phases from a code quality standpoint are **Phase 2** (HistoryManager), **Phase 3** (Services), and **Phase 6** (Controller). If time is limited, prioritise these three.

---

## 10. Definition of Done

The refactoring is complete when all of the following criteria are met:

### Size Criteria
- [ ] `main.cpp` is ≤ 70 lines and contains **only** application bootstrap and dependency wiring
- [ ] `EditorViewModel.cpp` is ≤ 150 lines and contains **no** undo/redo stack logic, no parsing logic, no serialization logic

### Module Criteria
- [ ] `PropertyCell` is in `src/views/PropertyCell.hpp/.cpp` and is theme-aware via `apply_style()`
- [ ] All translation dictionaries are in `src/i18n/TranslationLoader.cpp`, not in `main.cpp`
- [ ] All theme definitions are in `src/theme/ThemeRegistry.cpp`, not in `main.cpp`
- [ ] All menu building is in `src/menus/EditorMenuBuilder.cpp`, not in `main.cpp`
- [ ] All event wiring is in `src/controllers/EditorController.cpp`, not in `main.cpp`
- [ ] `AstService.cpp` is replaced by `AstSerializer`, `AstMutator`, `AstQuery` with interfaces

### Interface Criteria
- [ ] `IAstSerializer`, `IAstMutator`, `IAstQuery` interfaces exist as pure virtual classes
- [ ] `IDslCompilerService` interface exists as a pure virtual class
- [ ] `EditorViewModel` depends only on interfaces, not on concrete `AstService` statics
- [ ] `is_updating_` in `EditorViewModel` is **private**

### Quality Criteria
- [ ] Unit tests exist for `HistoryManager`, `AstSerializer`, `AstMutator`, `AstQuery`, `TranslationLoader`, `ThemeRegistry`
- [ ] The editor builds cleanly with `cmake --build build` (zero errors, zero new warnings)
- [ ] No AddressSanitizer errors or LeakSanitizer reports from the editor binary
- [ ] No hardcoded colors outside `ThemeRegistry.cpp`
- [ ] No hardcoded locale strings outside `TranslationLoader.cpp`

---

## Appendix A: Current vs. Target Line Count Estimate

| File | Current Lines | Target Lines |
|---|---|---|
| `main.cpp` | 849 | ~65 |
| `EditorViewModel.cpp` | 355 | ~150 |
| `AstService.cpp` | 184 | ~30 (deprecated facade) |
| `AstSerializer.cpp` | — (new) | ~70 |
| `AstMutator.cpp` | — (new) | ~130 |
| `AstQuery.cpp` | — (new) | ~30 |
| `HistoryManager.cpp` | — (new) | ~70 |
| `EditorController.cpp` | — (new) | ~180 |
| `EditorMenuBuilder.cpp` | — (new) | ~140 |
| `PropertyCell.cpp` | — (new) | ~130 |
| `PropertyGridConfigurator.cpp` | — (new) | ~80 |
| `ThemeRegistry.cpp` | — (new) | ~70 |
| `TranslationLoader.cpp` | — (new) | ~140 |
| `AppBootstrap.cpp` | — (new) | ~30 |
| `DslCompilerService.cpp` | — (new) | ~50 |

**Total lines**: ~1200 lines across ~16 focused files instead of ~1200 lines across 5 files.

The total line count does not decrease dramatically — boilerplate for interfaces and constructors is real. But **each file is singularly focused, understandable, and testable in isolation**. Reading `EditorController.cpp` tells you exactly how the editor responds to user interactions. Reading `ThemeRegistry.cpp` tells you exactly what the themes look like. No file does more than one job.

---

## Appendix B: Naming Convention for New Files

All new files follow the project's existing conventions per `docs/coding-standards.md`:

| Element | Convention | Example |
|---|---|---|
| Header file | `PascalCase.hpp` | `HistoryManager.hpp` |
| Source file | `PascalCase.cpp` | `HistoryManager.cpp` |
| Namespace | `snake_case` | `editor::services` |
| Class name | `PascalCase` | `class EditorController` |
| Interface | `IPascalCase` | `class IAstSerializer` |
| Public method | `snake_case()` | `void connect()` |
| Private member | `trailing_underscore_` | `std::shared_ptr<View> view_` |
| Test file | `test_snake_case.cpp` | `test_history_manager.cpp` |

