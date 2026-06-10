#pragma once
#include "tooey/ast.hpp"
#include <string>
#include <memory>

namespace editor::services {

class IAstQuery {
public:
    virtual ~IAstQuery() = default;

    virtual std::shared_ptr<tooey::AstNode> find_by_id(
        const std::shared_ptr<tooey::AstNode>& root,
        const std::string& id) const = 0;
};

} // namespace editor::services
