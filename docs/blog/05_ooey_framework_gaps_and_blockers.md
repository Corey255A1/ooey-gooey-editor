# Part 5: Core Gaps, Blockers, and Future Roadmap for the OOEY Framework

While we have succeeded in building a working split-view designer with live compilation and interactive selections, building a production-grade visual editor (comparable to Blend or Qt Creator) exposes several core limitations in the underlying **OOEY** framework. 

This final post analyzes the architectural limitations and missing features in the `ooey`, `gooey`, and `tooey` libraries, and proposes a technical roadmap to make the ecosystem a robust platform for visual builders.

---

## 1. Architectural Gap Analysis

### A. Lack of Runtime Reflection / Property Metadata
In C++, there is no native reflection. Unlike C# (used in WinForms) or QMetaMethod (used in Qt), the `ooey` framework lacks a standard way to query available properties of a control by string name at runtime.
* **The Blocker**: In [DynamicInterpreter.hpp](file:///home/corey/code/ooey-gooey-editor/src/DynamicInterpreter.hpp), we had to manually map properties (like `"text"`, `"checked"`, `"spacing"`) by casting controls to their concrete subclasses (e.g., `std::dynamic_pointer_cast<gooey::controls::Button>`). 
* **The Solution**: The `tooey` library should incorporate a reflection registry. Every widget should register its properties with a reflection class:
  ```cpp
  // Desired framework capability:
  auto properties = ElementRegistry::get_properties("Button");
  for (const auto& prop : properties) {
      grid.add_row(prop.name, prop.get_type());
  }
  ```

### B. Rigid Layout Constraints (No Freeform Canvas Dragging)
The layout system in `gooey` is built around structural layout components (`Column`, `Row`, `Grid`) that control child positions and sizes. 
* **The Blocker**: If a developer wants to drag a widget to resize it on the preview canvas, the parent container overrides that position during the layout pass. Making canvas items absolute-positioned (setting `is_absolute = true`) bypasses the layout engine entirely, but prevents the design from being responsive when running inside normal columns/rows.
* **The Solution**: We need an intermediate designer mode where layouts support margin offsets and constraints (similar to WPF's Anchor/Dock margins).

### C. Missing Drag-and-Drop Infrastructure
Building a drag-and-drop UX (e.g., dragging a label from the Tool Palette, moving it over the preview canvas, and seeing a placement preview) requires a coordinated event system.
* **The Blocker**: `gooey` lacks built-in drag-and-drop APIs. There is no standard way to:
  1. Start a drag gesture that tracks a visual ghost image of a widget.
  2. Query drag target elements for insertion capabilities.
  3. Draw placement indicator lines between elements during hover.
* **The Solution**: Implement `IDragSource` and `IDropTarget` interfaces in the core input engine, and dispatch drag events recursively.

### D. Input Event Routing: No Pointer Capture API
When resizing a widget by dragging a corner handle, the pointer frequently moves outside the bounds of the handle box itself.
* **The Blocker**: In most GUI frameworks, clicking a handle starts a **pointer capture** session (using methods like Win32 `SetCapture` or Qt `grabMouse`). This forces all mouse events to be routed to the handle until the mouse button is released, even if the mouse cursor leaves the handle's boundaries. In `gooey`, mouse events are dispatched purely based on coordinate hit-testing, meaning fast drags cause handles to lose focus and stop dragging.
* **The Solution**: Introduce `Application::set_pointer_capture(std::shared_ptr<GooeyElement>)` to lock input routing during drag gestures.

### E. Lack of Undo/Redo Transaction State
WYSIWYG tools rely on an Undo/Redo stack.
* **The Blocker**: The `tooey` AST library does not support transactional edits, snapshots, or delta serialization. Any change to the layout requires re-serializing the entire AST to a text DSL, which is expensive and prone to syntax errors.
* **The Solution**: Implement a command registry inside the AST manager to track structural changes (node additions, deletions, re-orderings) and values, enabling atomic undo/redo operations.

---

## 2. Technical Roadmap: Extending the Libraries

To address these limitations and support professional tools, we propose the following improvements:

```
┌────────────────────────────────────────────────────────┐
│              OOEY Core Enhancement Roadmap             │
├──────────────────────────┬─────────────────────────────┤
│ 1. Reflection Registry   │ Register setters/getters    │
│    (tooey/reflection.hpp)│ dynamically by string tag.  │
├──────────────────────────┼─────────────────────────────┤
│ 2. Drag & Drop API       │ Add IDragSource /           │
│    (gooey/input/dd.hpp)  │ IDropTarget interfaces.     │
├──────────────────────────┼─────────────────────────────┤
│ 3. Pointer Capture       │ Lock input dispatching to   │
│    (gooey/application)   │ active handle views.        │
├──────────────────────────┼─────────────────────────────┤
│ 4. Constraint Layout     │ Support anchoring, docking  │
│    (gooey/layouts)       │ and margin offsets.         │
└──────────────────────────┴─────────────────────────────┘
```

1. **Reflection Registry (`tooey`)**:
   Provide macro-based metadata definitions inside controls:
   ```cpp
   OOEY_PROPERTY(Button, std::string, label_text, set_label_text, get_label_text)
   ```
2. **Global Input Hooking**:
   Extend `gooey::Application` to support capturing pointers globally:
   ```cpp
   void set_active_pointer_capture(std::shared_ptr<GooeyElement> element);
   void release_pointer_capture();
   ```
3. **Advanced Grid Support**:
   Expand `gooey::Grid` layout rules to support cell spanning attributes (e.g. `Grid.ColumnSpan="2"`) within the parser, layout solver, and visual builders.

---

## 3. Conclusion

Building this visual builder demonstrates that while the `ooey` and `gooey` framework libraries provide a lightweight rendering and layout backend, expanding them to support visual design tools requires introducing higher-level layout constraints, input capture states, and runtime reflection systems.

By implementing the enhancements proposed in this roadmap, developers will be able to build not just simple applications, but highly complex, interactive visual creation tools fully integrated into the modern C++ GUI ecosystem.
