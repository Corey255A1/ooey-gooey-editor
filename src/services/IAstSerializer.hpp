#pragma once
#include "tooey/ast.hpp"
#include <string>
#include <memory>

namespace editor::services {

class IAstSerializer {
public:
    virtual ~IAstSerializer() = default;
    virtual std::string serialize(
        const std::shared_ptr<tooey::AstNode>& root,
        int indent = 0) const = 0;
};

} // namespace editor::services
