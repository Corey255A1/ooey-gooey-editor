#include "AstSerializer.hpp"
#include <sstream>
#include <algorithm>

namespace editor::services {

std::string AstSerializer::serialize(
    const std::shared_ptr<tooey::AstNode>& node,
    int indent) const {
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
            "paddingLeft", "paddingRight", "paddingTop", "paddingBottom",
            "text", "checked", "value", "placeholder", "color", "font",
            "title", "label", "labelText", "items"
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
                ss << serialize(child, indent + 1);
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
            ss << serialize(child, indent);
        }
    }

    return ss.str();
}

} // namespace editor::services
