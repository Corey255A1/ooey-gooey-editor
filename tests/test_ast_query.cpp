#include "services/AstQuery.hpp"
#include <cassert>
#include <iostream>

void test_ast_query() {
    editor::services::AstQuery query;

    // Null root
    assert(query.find_by_id(nullptr, "test") == nullptr);

    // Tree setup
    auto root = std::make_shared<tooey::AstNode>();
    root->id = "root";
    root->nodeType = "VBox";

    auto child1 = std::make_shared<tooey::AstNode>();
    child1->id = "child1";
    child1->nodeType = "Label";

    auto child2 = std::make_shared<tooey::AstNode>();
    child2->id = "child2";
    child2->nodeType = "Button";

    root->children.push_back(child1);
    root->children.push_back(child2);

    // Find root
    assert(query.find_by_id(root, "root") == root);

    // Find child1
    assert(query.find_by_id(root, "child1") == child1);

    // Find child2
    assert(query.find_by_id(root, "child2") == child2);

    // Find non-existent
    assert(query.find_by_id(root, "non_existent") == nullptr);
}

int main() {
    test_ast_query();
    std::cout << "All AstQuery tests passed successfully!\n";
    return 0;
}
