#pragma once

#include "tooey/ast.hpp"
#include "gooey/mvvmc/gooey_element.hpp"
#include "gooey/mvvmc/gooey_node.hpp"
#include "gooey/mvvmc/type_registry.hpp"
#include <memory>
#include <string>

namespace editor {

class DynamicInterpreter {
public:
    static std::shared_ptr<gooey::mvvmc::GooeyElement> interpret(const std::shared_ptr<tooey::AstNode>& node) {
        if (!node) return nullptr;

        std::shared_ptr<gooey::mvvmc::GooeyElement> element;

        if (node->nodeType == "Root") {
            element = std::make_shared<gooey::Column>();
        } else {
            element = gooey::TypeRegistry::get_instance().create(node->nodeType);
            if (!element) {
                element = std::make_shared<gooey::Column>(); // Fallback
            }
        }

        element->id = node->id;
        element->set_absolute(false);

        // Map AST property values to widget properties using TypeRegistry
        for (const auto& prop : node->properties) {
            gooey::TypeRegistry::get_instance().set_property(element, node->nodeType, prop.first, prop.second.rawData);
        }

        // Interpret child nodes recursively
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
