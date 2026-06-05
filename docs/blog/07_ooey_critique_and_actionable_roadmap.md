# Part 7: The Community Roast: Hard Critiques and an Actionable Roadmap

If you look at the current developer forums (Reddit’s `r/cpp` or StackOverflow), building a GUI builder with **OOEY** raises a lot of eyebrows. While the framework is praised for its lightweight rendering and clean layout compiler, developers trying to build real visual tools quickly hit architectural blockers.

This post compiles a series of harsh, realistic developer roasts of the OOEY design, paired with concrete, actionable tasks that must be implemented to make the visual builder production-ready.

---

## 1. The RTTI/Reflection Nightmare (The "C-with-Classes manual-cast hell")

### The Roast
> **User: `cpp_wizard_99` (Reddit)**  
> "Why on earth is this library calling itself a modern C++20 framework when it has *zero* property reflection? Look at [DynamicInterpreter.hpp](file:///home/corey/code/ooey-gooey-editor/src/DynamicInterpreter.hpp#L110-L128). It is literally a giant `if-else` chain casting elements manually. Have the authors never heard of variant mapping, registration patterns, or property tables? 
>
> If I want to add a new custom button or slider, I have to open the interpreter, add C++ type statements, manually write cast getters/setters, recompile the editor, and then recompile my layout tool. This is C++ in 2026, not C with classes in 1985! Without a runtime property registry, this framework is a hard blocker for any dynamic UI construction."

### The Actionable Task
Implement a centralized runtime type-registration and property reflection engine (`tooey::Reflection`).
* **Implementation Details**:
  1. Define a global `TypeRegistry` map matching control string tags (e.g. `"Button"`) to concrete factory methods and property maps.
  2. Register properties using property descriptors containing setter/getter functions:
     ```cpp
     // Register a property
     TypeRegistry::register_property<Button, std::string>(
         "text", 
         &Button::set_label_text, 
         &Button::get_label_text
     );
     ```
  3. Update `DynamicInterpreter` to instantiate nodes and apply property values dynamically using string keys, eliminating concrete subclass casts:
     ```cpp
     // Generic instantiation and mapping
     auto element = TypeRegistry::create("Button");
     TypeRegistry::set_property(element, "text", "Click Me");
     ```

---

## 2. The ScrollContainer Layout Trap (The "Silent Layout Collapse")

### The Roast
> **User: `segfault_pioneer` (StackOverflow)**  
> "Can we talk about the absolute disaster of `ScrollContainer` child layout? If you call `add_child` on a container, you expect it to lay out its child. But no! In OOEY, if you call `add_child` on a `ScrollContainer`, it compiles, it runs, but it renders a collapsed `0x0` box and doesn't tell you anything. You have to call `set_child` which takes a `std::shared_ptr<GooeyNode>`. 
>
> But the standard AST code generator outputs `add_child` because the compiler has zero parent-context type information. Why doesn't `add_child` override or throw an assert when called on a single-child container? Or better, why does a single-child container inherit from a multi-child `GooeyNode` base class if it cannot handle children array layouts properly? This is poor type hierarchy design."

### The Actionable Task
Refactor the class hierarchy to enforce single-child constraints at compile-time and runtime.
* **Implementation Details**:
  1. Create a `SingleChildContainer` base class inheriting from `GooeyElement` that manages exactly one child pointer (`child_`), exposing `set_child(std::shared_ptr<GooeyNode>)`.
  2. Make `ScrollContainer` inherit from `SingleChildContainer` instead of `GooeyNode`.
  3. If `GooeyNode` must be used as a base, override `add_child(std::shared_ptr<IDrawable>&& child)` inside `ScrollContainer` to throw a `std::runtime_error` or automatically redirect to `set_child(child)`. This prevents silent failures.

---

## 3. Visual Sizing Handles and Focus-Loss (The "Ice-Skating Cursor")

### The Roast
> **User: `ux_purist_xyz` (Reddit)**  
> "I'm trying to use the canvas stage overlay to resize a control by dragging a grip. The moment I drag my mouse faster than 2 pixels per millisecond, the cursor moves outside the tiny 6x6 grip boundary, hover focus is instantly lost, and the drag operation silently dies. 
>
> Are the framework developers unaware of pointer capture systems? Windows has `SetCapture`, Qt has `grabMouse`, HTML has `setPointerCapture`. The absolute lack of pointer capturing makes dragging controls or handles completely unusable on anything except a high-precision stylus."

### The Actionable Task
Implement a pointer capture tracking system in the core event loop.
* **Implementation Details**:
  1. Add a `pointer_capture_` field (a `std::weak_ptr<GooeyElement>`) to the `Application` and `Controller` class.
  2. Expose `set_pointer_capture(std::shared_ptr<GooeyElement> element)` and `release_pointer_capture()`.
  3. Modify the pointer event dispatcher: if `pointer_capture_` is active, route all pointer events (move, release, drag) directly to that element, regardless of hit-test boundaries, until pointer release occurs.

---

## 4. No Event Tunneling or Bubbling (The "Click-Through Void")

### The Roast
> **User: `event_handler_pro` (Reddit)**  
> "If I click on a button inside a nested visual stage container, the button intercept event gets swallowed, and the parent canvas never receives a bubble-up pointer click to let the designer highlight the parent layout. 
>
> Standard frameworks use tunneling (preview events) and bubbling. In OOEY, it is a binary 'handled or not' boolean returned by `on_pointer_event`. This is a hard blocker for writing any nested visual tree inspector or selection overlay."

### The Actionable Task
Implement a two-phase event routing system (Tunneling and Bubbling).
* **Implementation Details**:
  1. Introduce an `Event` class containing event status, type, and a `handled` or `stop_propagation()` flag.
  2. Implement the **Tunneling Phase**: traverse the parent-to-child route from the root view down to the event target, triggering `preview_pointer_event(Event&)`.
  3. Implement the **Bubbling Phase**: traverse back from target up to root, triggering `on_pointer_event(Event&)`. This allows ancestor canvas elements to intercept or monitor clicks on their children.

---

## 5. Summary of Actions for OOEY 2.0

Implementing these changes will transform OOEY from a basic UI rendering library into a developer-friendly, robust engine ready for complex visual construction tools:

| Feature / Issue | Community Critique | Actionable Remedy | Status |
| :--- | :--- | :--- | :--- |
| **Reflection** | Hardcoded types & casting | Centralized `TypeRegistry` and string-to-setter mapping | **Resolved** (Centralized `TypeRegistry` and dynamic mapping) |
| **Child Sizing** | Silent parent collapse | Refactor single-child containers; assert on invalid hierarchy additions | **Resolved** (Virtual `add_child` override in `ScrollContainer`) |
| **Resizing Grips** | Pointer loss on fast drag | Global `pointer_capture` locking events to handles | **Resolved** (Explicit `set_captured_element` in `Controller`) |
| **Undo/Redo** | No transactional edits or snapshots | Implement snapshot buffers and keyboard shortcuts | **Resolved** (Debounced text history & shortcut keys in RichTextBox) |
| **Diagnostics** | No live feedback on errors | Draw wavy squiggles for errors, duplicates, and localizations | **Resolved** (Lexer, duplicate ID, type, and localization checks) |
| **Event Routing** | Swallowed mouse clicks | Two-phase Event dispatch path (Tunneling & Bubbling) | Pending |
