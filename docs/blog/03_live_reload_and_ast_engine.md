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

## 3. Serializing the AST Back to Markup

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
