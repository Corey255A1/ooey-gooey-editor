#pragma once
#include "IAstMutator.hpp"
#include "IAstQuery.hpp"

namespace editor::services {

class AstMutator : public IAstMutator {
public:
    explicit AstMutator(std::shared_ptr<IAstQuery> query);

    bool add_child(
        const std::shared_ptr<tooey::AstNode>& root,
        const std::string& parent_id,
        const std::shared_ptr<tooey::AstNode>& new_node) override;

    bool delete_node(
        const std::shared_ptr<tooey::AstNode>& root,
        const std::string& target_id) override;

    bool move_node_up(
        const std::shared_ptr<tooey::AstNode>& root,
        const std::string& target_id) override;

    bool move_node_down(
        const std::shared_ptr<tooey::AstNode>& root,
        const std::string& target_id) override;

    bool update_properties(
        const std::shared_ptr<tooey::AstNode>& root,
        const std::string& target_id,
        const std::vector<std::pair<std::string, std::string>>& properties) override;

private:
    std::shared_ptr<IAstQuery> query_;
};

} // namespace editor::services
