#pragma once
#include <string>

namespace editor::domain {

struct ToolboxItem {
    std::string name;
};

struct HierarchyItem {
    std::string name;
    int         indent;
    std::string id;
};

struct PropertyItem {
    std::string name;
    std::string value;
};

} // namespace editor::domain
