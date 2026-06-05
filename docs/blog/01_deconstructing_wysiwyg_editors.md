# Part 1: Deconstructing Mainstream WYSIWYG Editors (Architectural Paradigms & UX)

For decades, Visual WYSIWYG (What You See Is What You Get) GUI designers have been the cornerstone of user interface development. By bridging the gap between designer intent and developer code, editors like Qt Creator, Visual Studio WinForms, and Expression Blend (WPF) redefined software construction. 

To build a comprehensive visual builder for the **OOEY-GOOEY** ecosystem, we must first study the giants that came before us. This post analyzes the architectural paradigms, UI-representation formats, and UX mechanics of mainstream visual builders, and establishes the blueprint for our custom editor.

---

## 1. Architectural Paradigms: How Mainstream Editors Work

Mainstream GUI builders can be categorized into three distinct architectural paradigms based on how they bridge the design canvas and the underlying code.

```
Qt Widgets (XML Template)  :  [.ui XML File]  ──(uic compiler)──► [C++ Header] ──► [Compile & Link]
WinForms (Code-Behind)     :  [Designer.cs]   ◄──(Two-Way Sync)──► [Visual Canvas]
WPF/Blend (Reactive XAML)  :  [.xaml Markup]  ◄──(Dynamic Parser)──► [Live Preview Canvas]
```

### A. QtWidgets: Static Template Compilation (`.ui` & `uic`)
Used in Qt Creator/Designer, this paradigm treats the UI design as an offline asset.
* **Markup Serialization**: The editor serializes widget positions, properties, and layouts into an XML-based `.ui` format.
* **Compilation Pipeline**: During the application's build process, the Qt User Interface Compiler (`uic`) parses the `.ui` file and generates a standard C++ header containing a layout configuration class (typically nested inside the `Ui` namespace, containing a setup function like `setupUi(QWidget*)`).
* **Pros**: Clean separation of design and logic; high run-time performance since layout code is fully compiled C++ with no run-time serialization or XML parsing.
* **Cons**: No live runtime modification of compiled views without rebuilding the application; UI bindings must be wired manually in code-behind subclasses.

### B. Windows Forms: Direct Code-Behind Serialization
The original WinForms editor in Visual Studio uses a direct-code paradigm.
* **Source Serialization**: The editor does not use an intermediate XML or JSON representation. Instead, when you drag a button on the visual canvas, the designer serializes that action directly into C# or C++ statements (e.g., `this.button1.Location = new System.Drawing.Point(12, 34);`) inside a dedicated method called `InitializeComponent()`.
* **Two-Way Synchronization**: When you open the designer, Visual Studio compiles the project in the background, instantiates the controls within a design-time shell, and uses reflection/metadata to render them.
* **Pros**: Simple to understand; what you edit visually maps one-to-one to executable initialization code.
* **Cons**: Fragile. Modifying `InitializeComponent()` manually frequently breaks the designer's parser, causing rendering failures. It is heavily dependent on language-specific reflection capabilities (like C# Roslyn or .NET RTTI).

### C. WPF/XAML and Blend: Reactive Markup Split-View
WPF and Blend introduce a highly decoupled, reactive markup model.
* **Dynamic Split-View**: WPF uses XAML (Extensible Application Markup Language), an XML dialect. The editor renders a split-screen view showing the markup code and a live designer.
* **Parsing Pipeline**: The designer runs a background compilation/interpreter loop. Any text edits in the XAML editor instantly compile down to a visual representation, and layout events are calculated on-the-fly. Conversely, moving elements on the canvas rewrites the corresponding XML nodes.
* **Pros**: Extremely robust. Designers can work purely in markup, and developers can work in code-behind. Highly dynamic layout support with advanced data bindings and styling.
* **Cons**: High memory overhead; requires a complex layout parser and binding engine to keep UI and code in sync.

---

## 2. Core Components of a WYSIWYG Builder

Regardless of the serialization format, every professional visual designer consists of four primary structural views that work in unison:

```
┌────────────────────────────────────────────────────────────────┐
│                       Visual UI Builder                        │
├───────────────────┬────────────────────────┬───────────────────┤
│                   │                        │                   │
│   Tool Palette    │   WYSIWYG Canvas       │  Visual Tree      │
│   [Button]        │   ┌────────────────┐   │  ▼ VBox           │
│   [Label]         │   │   [Button]     │   │    Label          │
│   [TextBox]       │   └────────────────┘   │    Button         │
│                   │                        │                   │
├───────────────────┴────────────────────────┼───────────────────┤
│               DSL / Code Editor            │ Property Inspector│
│  1: VBox:                                  │ ID:   btnSubmit   │
│  2:     Button id=btnSubmit text="Submit"  │ Text: Submit      │
│                                            │ Width: 120        │
└────────────────────────────────────────────┴───────────────────┘
```

1. **The Tool Palette (Toolbox)**: A registry of available widgets and layout containers. Dragging or selecting an element from the palette staging area signals the visual workspace to instantiate a new component.
2. **The WYSIWYG Preview Canvas**: The primary workspace. It renders the controls using their real styling, fonts, and dimensions. It intercepts normal pointer interactions (clicks, drags) to manage selection and resizing rather than triggering control execution (like clicking a button).
3. **The Visual Tree (Hierarchy Inspector)**: A tree view displaying the nesting hierarchy of the layout. It allows developers to select invisible layouts (like an empty column or spacer), reorder nodes, and group elements under containers.
4. **The Property Inspector (Grid)**: An interactive form that lists all properties of the selected element. It dynamically adapts its input controls based on the property type:
   * **Strings/Numbers**: Text boxes.
   * **Booleans**: Checkboxes or toggles.
   * **Enums (Alignments, Size Policies)**: Dropdowns.
   * **Colors**: Color pickers.

---

## 3. UX Design Mechanics: Interactive Manipulation

The value of a WYSIWYG editor lies in its interactive manipulation. To make an editor usable, we must implement several crucial UX features:

### A. Element Selection and the Selection Overlay
When a user clicks on an element within the canvas:
1. **Hit Testing**: The pointer coordinate is mapped to the control hierarchy to find the topmost element that encompasses the pointer.
2. **Focus State**: The selected element's ID is stored as the active focus.
3. **The Selection Overlay**: A dedicated drawing pass renders a highlighted rectangle slightly larger than the control's layout boundaries.
4. **Resizing Grips (Handles)**: Little handle boxes are drawn at the corners and midpoints of the selection outline. Dragging these handles modifies the element's width/height parameters.

### B. Layout Snapping and Guides
In container-based layouts (like vertical and horizontal boxes), element coordinates are not absolute. Instead, they are determined by their order in the child hierarchy, padding, and size policies. 
* **Insertion Indicators**: When dragging an element from the palette onto a `VBox` canvas, the editor must compute and draw a horizontal insertion bar indicating whether the element will be inserted before, between, or after existing children.
* **Alignment Guides (Grid Snapping)**: In absolute canvas layouts, virtual guidelines must snap elements to alignment lines (edges, centers) of neighboring widgets to maintain structural order.

### C. Split-Screen Synchronization and Change Logs
* **Bidirectional Updates**: When the developer types in the code editor, the AST (Abstract Syntax Tree) is rebuilt, and the preview canvas re-renders. If the parser encounters a syntax error, the canvas does not crash; it highlights the line and pauses updates until the code is valid again.
* **Undo/Redo History (Command Pattern)**: Every user action (dragging, typing, deleting) must be wrapped in a transactional command object. The editor maintains a history stack enabling developers to undo and redo visual changes seamlessly.

---

## 4. The Blueprint for the OOEY-GOOEY Editor

Our goal is to build an editor for `.ooey` layout files. Following our analysis of mainstream tools, we will design the OOEY-GOOEY editor around the **Split-Screen Interactive Markup Model** (similar to WPF/Blend).

* **Markup Format**: The OOEY layout format (handled by the `tooey` lexer and parser).
* **Live Interpreter**: A custom runtime interpreter that parses `.ooey` DSL text and instantiates a tree of `gooey::mvvmc::GooeyElement` nodes.
* **Interactive Layer**: A custom transparent `SelectionOverlay` that hooks pointer events to draw selection bounds and handle widget resizing.
* **Layout Design**: The editor's own user interface will be defined in `.ooey` markup, compiling to structured C++ classes using the `tooey_compiler`.

In the next post, we will build the foundation of the editor project, set up the build system, and design the editor's visual shell.
