#include "services/AstMutator.hpp"
#include "services/AstQuery.hpp"
#include <cassert>
#include <iostream>

void test_ast_mutator() {
    auto query = std::make_shared<editor::services::AstQuery>();
    editor::services::AstMutator mutator(query);

    // Setup initial tree
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

    // Add child to root
    auto child3 = std::make_shared<tooey::AstNode>();
    child3->id = "child3";
    child3->nodeType = "CheckBox";

    assert(mutator.add_child(root, "root", child3));
    assert(root->children.size() == 3);
    assert(root->children[2] == child3);

    // Add child to non-existent parent
    assert(!mutator.add_child(root, "non_existent", child3));

    // Move node up (child2 swaps with child1)
    assert(root->children[0]->id == "child1");
    assert(root->children[1]->id == "child2");
    assert(mutator.move_node_up(root, "child2"));
    assert(root->children[0]->id == "child2");
    assert(root->children[1]->id == "child1");

    // Move up on first child should fail/do nothing
    assert(!mutator.move_node_up(root, "child2"));

    // Move node down (child2 swaps with child1)
    assert(mutator.move_node_down(root, "child2"));
    assert(root->children[0]->id == "child1");
    assert(root->children[1]->id == "child2");

    // Move down on last child should fail/do nothing
    assert(!mutator.move_node_down(root, "child3"));

    // Update properties
    assert(mutator.update_properties(root, "child1", {{"text", "Updated"}, {"id", "new_child1"}}));
    assert(child1->id == "new_child1");
    assert(child1->properties["text"].rawData == "Updated");

    // Update non-existent node properties
    assert(!mutator.update_properties(root, "non_existent", {{"text", "Fail"}}));

    // Delete node
    assert(mutator.delete_node(root, "child2"));
    assert(root->children.size() == 2);
    assert(root->children[0]->id == "new_child1");
    assert(root->children[1]->id == "child3");

    // Delete non-existent node
    assert(!mutator.delete_node(root, "child2"));
}

int main() {
    test_ast_mutator();
    std::cout << "All AstMutator tests passed successfully!\n";
    return 0;
}
