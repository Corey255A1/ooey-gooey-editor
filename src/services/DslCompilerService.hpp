#pragma once
#include "IDslCompilerService.hpp"

namespace editor::services {

class DslCompilerService : public IDslCompilerService {
public:
    std::shared_ptr<tooey::AstNode> compile(const std::string& dsl) override;
    std::vector<LintDiagnostic> lint(const std::string& dsl) override;
};

} // namespace editor::services
