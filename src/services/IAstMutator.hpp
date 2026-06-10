#pragma once
#include "tooey/ast.hpp"
#include <string>
#include <vector>
#include <utility>
#include <memory>

namespace editor::services {

class IAstMutator {
public:
    virtual ~IAstMutator() = default;

    virtual bool add_child(
        const std::shared_ptr<tooey::AstNode>& root,
        const std::string& parent_id,
        const std::shared_ptr<tooey::AstNode>& new_node) = 0;

    virtual bool delete_node(
        const std::shared_ptr<tooey::AstNode>& root,
        const std::string& target_id) = 0;

    virtual bool move_node_up(
        const std::shared_ptr<tooey::AstNode>& root,
        const std::string& target_id) = 0;

    virtual bool move_node_down(
        const std::shared_ptr<tooey::AstNode>& root,
        const std::string& target_id) = 0;

    virtual bool update_properties(
        const std::shared_ptr<tooey::AstNode>& root,
        const std::string& target_id,
        const std::vector<std::pair<std::string, std::string>>& properties) = 0;
};

} // namespace editor::services
