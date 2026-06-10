#pragma once
#include "tooey/ast.hpp"
#include <string>
#include <memory>
#include <vector>
#include <cstdint>

namespace editor::services {

struct LintDiagnostic {
    int      line;
    int      start_col;
    int      end_col;
    uint32_t color_rgba; // packed RGBA
};

class IDslCompilerService {
public:
    virtual ~IDslCompilerService() = default;

    // Tokenizes + parses DSL. Returns nullptr on hard parse failure.
    virtual std::shared_ptr<tooey::AstNode> compile(const std::string& dsl) = 0;

    // Runs the linter only. Returns diagnostics.
    virtual std::vector<LintDiagnostic> lint(const std::string& dsl) = 0;
};

} // namespace editor::services
