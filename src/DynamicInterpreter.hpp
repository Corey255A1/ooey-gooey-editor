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

        if (node->nodeType == "VBox" || node->nodeType == "Column") {
            element = std::make_shared<gooey::Column>();
        } else if (node->nodeType == "HBox" || node->nodeType == "Row") {
            element = std::make_shared<gooey::Row>();
        } else if (node->nodeType == "Grid") {
            element = std::make_shared<gooey::Grid>(2, 2);
        } else if (node->nodeType == "FlowLayout") {
            element = std::make_shared<gooey::FlowLayout>();
        } else if (node->nodeType == "Button") {
            auto btn = std::make_shared<gooey::controls::Button>();
            element = btn;
        } else if (node->nodeType == "CheckBox") {
            auto cb = std::make_shared<gooey::controls::CheckBox>();
            element = cb;
        } else if (node->nodeType == "Label") {
            auto lbl = std::make_shared<gooey::controls::Label>();
            element = lbl;
        } else if (node->nodeType == "TextBox") {
            auto tb = std::make_shared<gooey::controls::TextBox>();
            element = tb;
        } else if (node->nodeType == "ScrollBar") {
            auto sb = std::make_shared<gooey::controls::ScrollBar>(ooey::Rect{0, 0, 15, 100}, gooey::controls::ScrollBarOrientation::Vertical);
            element = sb;
        } else if (node->nodeType == "ScrollContainer") {
            element = std::make_shared<gooey::controls::ScrollContainer>();
        } else if (node->nodeType == "ListControl") {
            element = std::make_shared<gooey::controls::ListControl>();
        } else if (node->nodeType == "Root") {
            element = std::make_shared<gooey::Column>();
        } else {
            element = std::make_shared<gooey::Column>();
        }

        element->id = node->id;
        element->set_absolute(false);

        // Map AST property values to widget properties using setters
        for (const auto& prop : node->properties) {
            std::string key = prop.first;
            std::string val = prop.second.rawData;

            if (key == "text") {
                if (node->nodeType == "Button") {
                    std::dynamic_pointer_cast<gooey::controls::Button>(element)->set_label_text(val);
                } else if (node->nodeType == "Label") {
                    std::dynamic_pointer_cast<gooey::controls::Label>(element)->set_text(val);
                } else if (node->nodeType == "CheckBox") {
                    std::dynamic_pointer_cast<gooey::controls::CheckBox>(element)->set_label_text(val);
                } else if (node->nodeType == "TextBox") {
                    std::dynamic_pointer_cast<gooey::controls::TextBox>(element)->set_text(val);
                }
            } else if (key == "checked") {
                if (node->nodeType == "CheckBox") {
                    std::dynamic_pointer_cast<gooey::controls::CheckBox>(element)->set_checked(val == "true");
                }
            } else if (key == "spacing") {
                if (node->nodeType == "VBox" || node->nodeType == "Column") {
                    std::dynamic_pointer_cast<gooey::Column>(element)->set_spacing(std::stoi(val));
                } else if (node->nodeType == "HBox" || node->nodeType == "Row") {
                    std::dynamic_pointer_cast<gooey::Row>(element)->set_spacing(std::stoi(val));
                }
            } else if (key == "width") {
                if (val == "MatchParent") {
                    element->set_width(gooey::SizePolicy::MatchParent);
                } else if (val == "WrapContent") {
                    element->set_width(gooey::SizePolicy::WrapContent);
                } else {
                    element->set_width(gooey::SizePolicy::Fixed, std::stoi(val));
                }
            } else if (key == "height") {
                if (val == "MatchParent") {
                    element->set_height(gooey::SizePolicy::MatchParent);
                } else if (val == "WrapContent") {
                    element->set_height(gooey::SizePolicy::WrapContent);
                } else {
                    element->set_height(gooey::SizePolicy::Fixed, std::stoi(val));
                }
            }
        }

        // Interpret child nodes recursively
        auto node_element = std::dynamic_pointer_cast<gooey::mvvmc::GooeyNode>(element);
        if (node_element) {
            for (const auto& child : node->children) {
                auto child_element = interpret(child);
                if (child_element) {
                    node_element->add_child(child_element);
                }
            }
        }

        return element;
    }
};

} // namespace editor
