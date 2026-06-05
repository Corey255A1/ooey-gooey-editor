#include "AstService.hpp"
#include <sstream>
#include <cctype>
#include <algorithm>

namespace editor {

std::string AstService::serialize_ast(const std::shared_ptr<tooey::AstNode>& node, int indent) {
    if (!node) return "";
    std::stringstream ss;

    if (node->nodeType != "Root") {
        std::string indent_str(indent * 4, ' ');
        ss << indent_str << node->nodeType;
        if (!node->id.empty()) {
            ss << " id=" << node->id;
        }
        
        static const std::vector<std::string> inline_props = {
            "width", "height", "spacing", "margin", "padding",
            "alignSelf", "alignItems", "justifyContent",
            "minWidth", "maxWidth", "minHeight", "maxHeight",
            "marginLeft", "marginRight", "marginTop", "marginBottom",
            "paddingLeft", "paddingRight", "paddingTop", "paddingBottom"
        };
        for (const auto& prop_name : inline_props) {
            auto it = node->properties.find(prop_name);
            if (it != node->properties.end()) {
                std::string val = it->second.rawData;
                if (it->second.type == tooey::PropertyType::String) {
                    val = "\"" + val + "\"";
                }
                ss << " " << prop_name << "=" << val;
            }
        }

        if (!node->children.empty()) {
            ss << ":\n";
            for (const auto& child : node->children) {
                ss << serialize_ast(child, indent + 1);
            }
        } else {
            ss << "\n";
            for (const auto& prop : node->properties) {
                if (std::find(inline_props.begin(), inline_props.end(), prop.first) == inline_props.end()) {
                    std::string val = prop.second.rawData;
                    if (prop.second.type == tooey::PropertyType::String) {
                        val = "\"" + val + "\"";
                    } else if (prop.second.type == tooey::PropertyType::Localization) {
                        val = "@tr(\"" + val + "\")";
                    } else if (prop.second.type == tooey::PropertyType::Binding) {
                        val = "@binding." + val;
                    } else if (prop.second.type == tooey::PropertyType::Signal) {
                        val = "@signal." + val;
                    }
                    ss << indent_str << "    " << prop.first << ": " << val << "\n";
                }
            }
        }
    } else {
        for (const auto& child : node->children) {
            ss << serialize_ast(child, indent);
        }
    }

    return ss.str();
}

bool AstService::add_child_node(const std::shared_ptr<tooey::AstNode>& root, const std::string& parentId, const std::shared_ptr<tooey::AstNode>& newNode) {
    if (!root) return false;
    if (root->id == parentId) {
        root->children.push_back(newNode);
        return true;
    }
    for (const auto& child : root->children) {
        if (add_child_node(child, parentId, newNode)) return true;
    }
    return false;
}

bool AstService::delete_node(const std::shared_ptr<tooey::AstNode>& root, const std::string& targetId) {
    if (!root) return false;
    for (auto it = root->children.begin(); it != root->children.end(); ++it) {
        if ((*it)->id == targetId) {
            root->children.erase(it);
            return true;
        }
        if (delete_node(*it, targetId)) return true;
    }
    return false;
}

bool AstService::move_node_up(const std::shared_ptr<tooey::AstNode>& root, const std::string& targetId) {
    if (!root) return false;
    for (auto it = root->children.begin(); it != root->children.end(); ++it) {
        if ((*it)->id == targetId) {
            if (it != root->children.begin()) {
                std::iter_swap(it, it - 1);
                return true;
            }
            return false;
        }
        if (move_node_up(*it, targetId)) return true;
    }
    return false;
}

bool AstService::move_node_down(const std::shared_ptr<tooey::AstNode>& root, const std::string& targetId) {
    if (!root) return false;
    for (auto it = root->children.begin(); it != root->children.end(); ++it) {
        if ((*it)->id == targetId) {
            if (it + 1 != root->children.end()) {
                std::iter_swap(it, it + 1);
                return true;
            }
            return false;
        }
        if (move_node_down(*it, targetId)) return true;
    }
    return false;
}

std::shared_ptr<tooey::AstNode> AstService::find_node_by_id(const std::shared_ptr<tooey::AstNode>& root, const std::string& targetId) {
    if (!root) return nullptr;
    if (root->id == targetId) return root;
    for (const auto& child : root->children) {
        auto found = find_node_by_id(child, targetId);
        if (found) return found;
    }
    return nullptr;
}

bool AstService::update_node_properties(const std::shared_ptr<tooey::AstNode>& root, const std::string& targetId, const std::vector<std::pair<std::string, std::string>>& properties) {
    auto node = find_node_by_id(root, targetId);
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

} // namespace editor
