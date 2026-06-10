#pragma once
#include "IAstSerializer.hpp"

namespace editor::services {

class AstSerializer : public IAstSerializer {
public:
    std::string serialize(
        const std::shared_ptr<tooey::AstNode>& root,
        int indent = 0) const override;
};

} // namespace editor::services
