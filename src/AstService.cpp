#include "AstService.hpp"
#include "services/AstSerializer.hpp"
#include "services/AstQuery.hpp"
#include "services/AstMutator.hpp"

namespace editor {

std::string AstService::serialize_ast(const std::shared_ptr<tooey::AstNode>& node, int indent) {
    return services::AstSerializer{}.serialize(node, indent);
}

bool AstService::add_child_node(const std::shared_ptr<tooey::AstNode>& root, const std::string& parentId, const std::shared_ptr<tooey::AstNode>& newNode) {
    auto query = std::make_shared<services::AstQuery>();
    return services::AstMutator{query}.add_child(root, parentId, newNode);
}

bool AstService::delete_node(const std::shared_ptr<tooey::AstNode>& root, const std::string& targetId) {
    auto query = std::make_shared<services::AstQuery>();
    return services::AstMutator{query}.delete_node(root, targetId);
}

bool AstService::move_node_up(const std::shared_ptr<tooey::AstNode>& root, const std::string& targetId) {
    auto query = std::make_shared<services::AstQuery>();
    return services::AstMutator{query}.move_node_up(root, targetId);
}

bool AstService::move_node_down(const std::shared_ptr<tooey::AstNode>& root, const std::string& targetId) {
    auto query = std::make_shared<services::AstQuery>();
    return services::AstMutator{query}.move_node_down(root, targetId);
}

std::shared_ptr<tooey::AstNode> AstService::find_node_by_id(const std::shared_ptr<tooey::AstNode>& root, const std::string& targetId) {
    return services::AstQuery{}.find_by_id(root, targetId);
}

bool AstService::update_node_properties(const std::shared_ptr<tooey::AstNode>& root, const std::string& targetId, const std::vector<std::pair<std::string, std::string>>& properties) {
    auto query = std::make_shared<services::AstQuery>();
    return services::AstMutator{query}.update_properties(root, targetId, properties);
}

} // namespace editor
