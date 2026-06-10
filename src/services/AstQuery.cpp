#include "AstQuery.hpp"

namespace editor::services {

std::shared_ptr<tooey::AstNode> AstQuery::find_by_id(
    const std::shared_ptr<tooey::AstNode>& root,
    const std::string& id) const {
    if (!root) return nullptr;
    if (root->id == id) return root;
    for (const auto& child : root->children) {
        auto found = find_by_id(child, id);
        if (found) return found;
    }
    return nullptr;
}

} // namespace editor::services
