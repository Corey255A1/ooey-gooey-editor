#include "services/AstSerializer.hpp"
#include <cassert>
#include <iostream>

void test_ast_serializer() {
    editor::services::AstSerializer serializer;

    // Null AST
    assert(serializer.serialize(nullptr) == "");

    // Single-node AST (with inline property)
    auto node = std::make_shared<tooey::AstNode>();
    node->nodeType = "Label";
    node->id = "lbl1";
    node->properties["text"] = {tooey::PropertyType::String, "Hello"};

    std::string expected_single = "Label id=lbl1 text=\"Hello\"\n";
    assert(serializer.serialize(node) == expected_single);

    // Single-node with non-inline property (e.g. custom user property)
    auto node_custom = std::make_shared<tooey::AstNode>();
    node_custom->nodeType = "Button";
    node_custom->id = "btn1";
    node_custom->properties["custom_prop"] = {tooey::PropertyType::String, "Val"};
    
    std::string expected_custom = "Button id=btn1\n    custom_prop: \"Val\"\n";
    assert(serializer.serialize(node_custom) == expected_custom);

    // Nested nodes with 4-space indent
    auto parent = std::make_shared<tooey::AstNode>();
    parent->nodeType = "VBox";
    parent->id = "box";
    parent->children.push_back(node);

    std::string expected_nested = 
        "VBox id=box:\n"
        "    Label id=lbl1 text=\"Hello\"\n";
    assert(serializer.serialize(parent) == expected_nested);

    // Localization and binding syntax
    auto node_special = std::make_shared<tooey::AstNode>();
    node_special->nodeType = "Button";
    node_special->id = "btn_special";
    node_special->properties["custom_tr"] = {tooey::PropertyType::Localization, "menu_file"};
    node_special->properties["custom_bind"] = {tooey::PropertyType::Binding, "active_locale"};

    std::string expected_special = 
        "Button id=btn_special\n"
        "    custom_bind: @binding.active_locale\n"
        "    custom_tr: @tr(\"menu_file\")\n";
    assert(serializer.serialize(node_special) == expected_special);
}

int main() {
    test_ast_serializer();
    std::cout << "All AstSerializer tests passed successfully!\n";
    return 0;
}
