#include "AstMutator.hpp"
#include <algorithm>
#include <cctype>

namespace editor::services {

AstMutator::AstMutator(std::shared_ptr<IAstQuery> query)
    : query_(query)
{}

bool AstMutator::add_child(
    const std::shared_ptr<tooey::AstNode>& root,
    const std::string& parent_id,
    const std::shared_ptr<tooey::AstNode>& new_node) {
    if (!root) return false;
    if (root->id == parent_id) {
        root->children.push_back(new_node);
        return true;
    }
    for (const auto& child : root->children) {
        if (add_child(child, parent_id, new_node)) return true;
    }
    return false;
}

bool AstMutator::delete_node(
    const std::shared_ptr<tooey::AstNode>& root,
    const std::string& target_id) {
    if (!root) return false;
    for (auto it = root->children.begin(); it != root->children.end(); ++it) {
        if ((*it)->id == target_id) {
            root->children.erase(it);
            return true;
        }
        if (delete_node(*it, target_id)) return true;
    }
    return false;
}

bool AstMutator::move_node_up(
    const std::shared_ptr<tooey::AstNode>& root,
    const std::string& target_id) {
    if (!root) return false;
    for (auto it = root->children.begin(); it != root->children.end(); ++it) {
        if ((*it)->id == target_id) {
            if (it != root->children.begin()) {
                std::iter_swap(it, it - 1);
                return true;
            }
            return false;
        }
        if (move_node_up(*it, target_id)) return true;
    }
    return false;
}

bool AstMutator::move_node_down(
    const std::shared_ptr<tooey::AstNode>& root,
    const std::string& target_id) {
    if (!root) return false;
    for (auto it = root->children.begin(); it != root->children.end(); ++it) {
        if ((*it)->id == target_id) {
            if (it + 1 != root->children.end()) {
                std::iter_swap(it, it + 1);
                return true;
            }
            return false;
        }
        if (move_node_down(*it, target_id)) return true;
    }
    return false;
}

bool AstMutator::update_properties(
    const std::shared_ptr<tooey::AstNode>& root,
    const std::string& target_id,
    const std::vector<std::pair<std::string, std::string>>& properties) {
    
    auto node = query_->find_by_id(root, target_id);
    if (!node) return false;

    bool changed = false;
    for (const auto& item : properties) {
        if (item.first == "id") {
            if (node->id != item.second) {
                node->id = item.second;
                changed = true;
            }
        } else {
            tooey::PropertyValue new_val;
            std::string val = item.second;
            if (val == "true" || val == "false") {
                new_val.type = tooey::PropertyType::Boolean;
            } else if (val.starts_with("@binding.")) {
                new_val.type = tooey::PropertyType::Binding;
                val = val.substr(9);
            } else if (val.starts_with("@signal.")) {
                new_val.type = tooey::PropertyType::Signal;
                val = val.substr(8);
            } else if (val.starts_with("@tr(")) {
                new_val.type = tooey::PropertyType::Localization;
                size_t first = val.find("\"");
                size_t last = val.rfind("\"");
                if (first != std::string::npos && last != std::string::npos && last > first) {
                    val = val.substr(first + 1, last - first - 1);
                }
            } else {
                bool is_num = true;
                for (char c : val) {
                    if (!std::isdigit(c)) { is_num = false; break; }
                }
                new_val.type = is_num ? tooey::PropertyType::Number : tooey::PropertyType::String;
            }
            new_val.rawData = val;

            auto it = node->properties.find(item.first);
            if (it == node->properties.end() || it->second.rawData != new_val.rawData || it->second.type != new_val.type) {
                node->properties[item.first] = new_val;
                changed = true;
            }
        }
    }
    return changed;
}

} // namespace editor::services
