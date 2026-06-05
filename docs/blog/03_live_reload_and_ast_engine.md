# Part 3: The Live-Reload Pipeline & AST Translation Engine

At the core of a modern WYSIWYG layout builder is a high-performance translation pipeline. When a developer types in the code editor, the visual canvas must reflect the changes instantly. Conversely, when a visual control is edited, added, or resized, the raw code must update seamlessly. 

This post details the bidirectional live-reload engine, highlighting the AST translation system that instantiates runtime GUI components from markup metadata.

---

## 1. The Bidirectional Synchronization Loop

To keep the visual preview canvas and the text editor synchronized without conflicts, we establish a single-source-of-truth loop managed by our `EditorViewModel`:

```
 [ RichTextBox Editor ]                                        [ Live Preview Canvas ]
          │ (on_text_changed)                                             ▲
          ▼                                                               │
┌───────────────────┐     ┌────────────────┐     ┌───────────────┐        │ (re-render)
│   dslText State   ├────►│ tooey::Parser  ├────►│  Interpreter  ├────────┘
└─────────▲─────────┘     └────────────────┘     └───────────────┘
          │ (serialize)
   [ VM AST Mutations ]  ◄──(add / delete / properties)──  [ Interaction Overlays ]
```

1. **Text-to-Visual (Forward Pipeline)**:
   * Typing in the code editor fires a change signal that updates the view model's reactive `dslText` property.
   * `dslText` has an active observer that tokenizes the text via `tooey::Lexer` and parses it into an AST via `tooey::Parser`.
   * If parsing is successful, the AST is passed to the `DynamicInterpreter`, which traverses the tree and instantiates corresponding C++ layout/widget elements.
   * The new dynamic layout tree replaces the children of the canvas container.
2. **Visual-to-Text (Reverse Pipeline)**:
   * Visual actions (adding elements via the palette, deleting items, or typing in the property grid) modify the AST representation directly.
   * A serializer traverses the modified AST, converts it back into formatted `.ooey` layout text, and writes it to `dslText`.
   * The text update propagates back to the editor, updating the visual preview.

To prevent circular dependency feedback loops (e.g. text updates triggering layout updates which trigger serializations), the view model guards execution with an `is_updating_` boolean flag.

---

## 2. Implementing the AST Interpreter (`DynamicInterpreter`)

Since C++ lacks native runtime reflection or string-to-class constructors (unlike Java or C#), we must build a factory mapping parser tags to GUI elements. The class traverses the tree recursively, constructs the hierarchy, and maps property values.

Here is the implementation of `DynamicInterpreter.hpp`:

```cpp
#pragma once
#include "tooey/ast.hpp"
#include "gooey/mvvmc/gooey_element.hpp"
#include "gooey/mvvmc/gooey_node.hpp"
#include "gooey/controls/button.hpp"
#include "gooey/controls/checkbox.hpp"
#include "gooey/controls/label.hpp"
#include "gooey/controls/text_box.hpp"
#include "gooey/controls/column.hpp"
#include "gooey/controls/row.hpp"
#include "gooey/controls/grid.hpp"
#include "gooey/controls/flow_layout.hpp"
#include "gooey/controls/scrollbar.hpp"
#include "gooey/controls/scroll_container.hpp"
#include "gooey/controls/list_control.hpp"
#include <memory>
#include <string>

namespace editor {

class DynamicInterpreter {
public:
    static std::shared_ptr<gooey::mvvmc::GooeyElement> interpret(const std::shared_ptr<tooey::AstNode>& node) {
        if (!node) return nullptr;

        std::shared_ptr<gooey::mvvmc::GooeyElement> element;

        // 1. Tag-to-Class Factory Instantiation
        if (node->nodeType == "VBox" || node->nodeType == "Column") {
            element = std::make_shared<gooey::Column>();
        } else if (node->nodeType == "HBox" || node->nodeType == "Row") {
            element = std::make_shared<gooey::Row>();
        } else if (node->nodeType == "Grid") {
            element = std::make_shared<gooey::Grid>(2, 2);
        } else if (node->nodeType == "FlowLayout") {
            element = std::make_shared<gooey::FlowLayout>();
        } else if (node->nodeType == "Button") {
            element = std::make_shared<gooey::controls::Button>();
        } else if (node->nodeType == "CheckBox") {
            element = std::make_shared<gooey::controls::CheckBox>();
        } else if (node->nodeType == "Label") {
            element = std::make_shared<gooey::controls::Label>();
        } else if (node->nodeType == "TextBox") {
            element = std::make_shared<gooey::controls::TextBox>();
        } else if (node->nodeType == "ScrollBar") {
            element = std::make_shared<gooey::controls::ScrollBar>(
                ooey::Rect{0, 0, 15, 100}, 
                gooey::controls::ScrollBarOrientation::Vertical
            );
        } else if (node->nodeType == "ScrollContainer") {
            element = std::make_shared<gooey::controls::ScrollContainer>();
        } else if (node->nodeType == "ListControl") {
            element = std::make_shared<gooey::controls::ListControl>();
        } else {
            element = std::make_shared<gooey::Column>(); // Fallback
        }

        element->id = node->id;
        element->set_absolute(false);

        // 2. Property Attribute Mapping
        for (const auto& prop : node->properties) {
            std::string key = prop.first;
            std::string val = prop.second.rawData;

            if (key == "text") {
                if (auto btn = std::dynamic_pointer_cast<gooey::controls::Button>(element)) btn->set_label_text(val);
                else if (auto lbl = std::dynamic_pointer_cast<gooey::controls::Label>(element)) lbl->set_text(val);
                else if (auto cb = std::dynamic_pointer_cast<gooey::controls::CheckBox>(element)) cb->set_label_text(val);
                else if (auto tb = std::dynamic_pointer_cast<gooey::controls::TextBox>(element)) tb->set_text(val);
            } else if (key == "checked" && node->nodeType == "CheckBox") {
                std::dynamic_pointer_cast<gooey::controls::CheckBox>(element)->set_checked(val == "true");
            } else if (key == "spacing") {
                if (auto col = std::dynamic_pointer_cast<gooey::Column>(element)) col->set_spacing(std::stoi(val));
                else if (auto row = std::dynamic_pointer_cast<gooey::Row>(element)) row->set_spacing(std::stoi(val));
            } else if (key == "width") {
                if (val == "MatchParent") element->set_width(gooey::SizePolicy::MatchParent);
                else if (val == "WrapContent") element->set_width(gooey::SizePolicy::WrapContent);
                else element->set_width(gooey::SizePolicy::Fixed, std::stoi(val));
            } else if (key == "height") {
                if (val == "MatchParent") element->set_height(gooey::SizePolicy::MatchParent);
                else if (val == "WrapContent") element->set_height(gooey::SizePolicy::WrapContent);
                else element->set_height(gooey::SizePolicy::Fixed, std::stoi(val));
            }
        }

        // 3. Recursive Child Composition (Handling ScrollContainer Constraints)
        auto scroll_container = std::dynamic_pointer_cast<gooey::controls::ScrollContainer>(element);
        if (scroll_container) {
            for (const auto& child : node->children) {
                auto child_element = interpret(child);
                if (child_element) {
                    auto child_node = std::dynamic_pointer_cast<gooey::mvvmc::GooeyNode>(child_element);
                    if (child_node) {
                        scroll_container->set_child(child_node);
                    }
                }
            }
        } else {
            auto node_element = std::dynamic_pointer_cast<gooey::mvvmc::GooeyNode>(element);
            if (node_element) {
                for (const auto& child : node->children) {
                    auto child_element = interpret(child);
                    if (child_element) {
                        node_element->add_child(child_element);
                    }
                }
            }
        }

        return element;
    }
};

} // namespace editor
```

---

## 3. AST Mutation Operations: Programmatic Node Modification

For a visual editor to be fully functional, visual canvas modifications must mutate the in-memory AST representation before trigger serialization. Here is how our `EditorViewModel` performs atomic AST modifications:

### A. Adding a Widget
When a user selects a widget class in the Tool Palette and clicks **Add Selected**, the view model instantiates a default AST node, assigns a unique ID, and appends it to the currently selected container node (or appends it to the root column if nothing is selected):

```cpp
void EditorViewModel::addControlToCanvas(const std::string& type) {
    auto newNode = std::make_shared<tooey::AstNode>();
    newNode->nodeType = type;
    newNode->id = "new" + type + "_" + std::to_string(rand() % 1000);
    
    // Assign default property attributes based on control type
    if (type == "Label") {
        newNode->properties["text"] = {tooey::PropertyType::String, "New Label"};
    } else if (type == "Button") {
        newNode->properties["text"] = {tooey::PropertyType::String, "New Button"};
    } else if (type == "CheckBox") {
        newNode->properties["text"] = {tooey::PropertyType::String, "New CheckBox"};
    }

    struct Insertion {
        std::string targetId;
        std::shared_ptr<tooey::AstNode> newNode;
        
        bool add(const std::shared_ptr<tooey::AstNode>& n) {
            if (!n) return false;
            if (n->id == targetId) {
                n->children.push_back(newNode); // Append child
                return true;
            }
            for (const auto& child : n->children) {
                if (add(child)) return true;
            }
            return false;
        }
    };

    bool added = false;
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(hierarchyItems.get().size())) {
        Insertion ins{hierarchyItems.get()[selectedIndex].id, newNode};
        added = ins.add(currentAst);
    }
    if (!added) {
        if (!currentAst->children.empty()) {
            currentAst->children[0]->children.push_back(newNode);
        } else {
            currentAst->children.push_back(newNode);
        }
    }

    // Trigger full code regeneration and preview canvas reload
    std::string serialized = generateDslFromAst(currentAst, 0);
    dslText.set(serialized);
    updateCanvasFromDsl(serialized);
}
```

### B. Deleting a Widget
Deleting the selected control requires finding the node's parent in the AST hierarchy and removing the node from the parent's children vector:

```cpp
void EditorViewModel::deleteSelectedElement() {
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(hierarchyItems.get().size())) return;
    auto targetId = hierarchyItems.get()[selectedIndex].id;
    if (targetId == "mainLayout") return; // Root container cannot be deleted

    struct Deleter {
        std::string targetId;
        bool delete_node(const std::shared_ptr<tooey::AstNode>& parent) {
            if (!parent) return false;
            for (auto it = parent->children.begin(); it != parent->children.end(); ++it) {
                if ((*it)->id == targetId) {
                    parent->children.erase(it); // Erase from parent vector
                    return true;
                }
                if (delete_node(*it)) return true;
            }
            return false;
        }
    };

    Deleter del{targetId};
    if (del.delete_node(currentAst)) {
        selectedIndex = -1;
        is_updating_ = true;
        std::string serialized = generateDslFromAst(currentAst, 0);
        dslText.set(serialized);
        updateCanvasFromDsl(serialized);
        is_updating_ = false;
    }
}
```

### C. Reordering Items (Moving Up and Down)
Reordering elements in structural layouts is achieved by changing their index positions inside their parent's children list:

```cpp
void EditorViewModel::moveSelectedElementUp() {
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(hierarchyItems.get().size())) return;
    auto targetId = hierarchyItems.get()[selectedIndex].id;
    if (targetId == "mainLayout") return;

    struct Mover {
        std::string targetId;
        bool move_up(const std::shared_ptr<tooey::AstNode>& parent) {
            if (!parent) return false;
            for (auto it = parent->children.begin(); it != parent->children.end(); ++it) {
                if ((*it)->id == targetId) {
                    if (it != parent->children.begin()) {
                        std::iter_swap(it, it - 1); // Swap with previous element
                        return true;
                    }
                    return false;
                }
                if (move_up(*it)) return true;
            }
            return false;
        }
    };

    Mover mov{targetId};
    if (mov.move_up(currentAst)) {
        is_updating_ = true;
        std::string serialized = generateDslFromAst(currentAst, 0);
        dslText.set(serialized);
        updateCanvasFromDsl(serialized);
        // Retain selection focus on active moved item
        for (size_t i = 0; i < hierarchyItems.get().size(); ++i) {
            if (hierarchyItems.get()[i].id == targetId) {
                selectedIndex = static_cast<int>(i);
                break;
            }
        }
        is_updating_ = false;
    }
}
```

---

## 4. Serializing the AST Back to Markup

When a widget's property changes inside the property grid, we write those changes back to the AST and generate the output code. The serialization function traverses the node tree recursively and formats the output string:

```cpp
std::string EditorViewModel::generateDslFromAst(const std::shared_ptr<tooey::AstNode>& node, int indent) {
    if (!node || node->nodeType == "Root") {
        std::string out = "";
        for (const auto& child : node->children) {
            out += generateDslFromAst(child, indent);
        }
        return out;
    }

    std::stringstream ss;
    std::string spaces(indent * 4, ' ');

    ss << spaces << node->nodeType;
    if (!node->id.empty()) {
        ss << " id=" << node->id;
    }

    // Format properties
    for (const auto& prop : node->properties) {
        ss << " " << prop.first << "=";
        if (prop.second.type == tooey::PropertyType::String) {
            ss << "\"" << prop.second.rawData << "\"";
        } else if (prop.second.type == tooey::PropertyType::Binding) {
            ss << "@binding." << prop.second.rawData;
        } else if (prop.second.type == tooey::PropertyType::Signal) {
            ss << "@signal." << prop.second.rawData;
        } else if (prop.second.type == tooey::PropertyType::Localization) {
            ss << "@tr(\"" << prop.second.rawData << "\")";
        } else {
            ss << prop.second.rawData;
        }
    }

    if (!node->children.empty()) {
        ss << ":\n";
        for (const auto& child : node->children) {
            ss << generateDslFromAst(child, indent + 1);
        }
    } else {
        ss << "\n";
    }

    return ss.str();
}
```

In the next post, we will explore the visual interaction layer: implementing mouse selection click-tests, element highlighting, and property editing components.
