#pragma once

#include "tooey/ast.hpp"
#include <string>
#include <vector>
#include <utility>
#include <memory>

namespace editor {

class AstService {
public:
    static std::string serialize_ast(const std::shared_ptr<tooey::AstNode>& node, int indent = 0);
    static bool add_child_node(const std::shared_ptr<tooey::AstNode>& root, const std::string& parentId, const std::shared_ptr<tooey::AstNode>& newNode);
    static bool delete_node(const std::shared_ptr<tooey::AstNode>& root, const std::string& targetId);
    static bool move_node_up(const std::shared_ptr<tooey::AstNode>& root, const std::string& targetId);
    static bool move_node_down(const std::shared_ptr<tooey::AstNode>& root, const std::string& targetId);
    static std::shared_ptr<tooey::AstNode> find_node_by_id(const std::shared_ptr<tooey::AstNode>& root, const std::string& targetId);
    static bool update_node_properties(const std::shared_ptr<tooey::AstNode>& root, const std::string& targetId, const std::vector<std::pair<std::string, std::string>>& properties);
};

} // namespace editor
