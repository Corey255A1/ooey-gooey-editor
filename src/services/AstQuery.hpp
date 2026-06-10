#pragma once
#include "IAstQuery.hpp"

namespace editor::services {

class AstQuery : public IAstQuery {
public:
    std::shared_ptr<tooey::AstNode> find_by_id(
        const std::shared_ptr<tooey::AstNode>& root,
        const std::string& id) const override;
};

} // namespace editor::services
