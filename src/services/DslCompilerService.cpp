#include "DslCompilerService.hpp"
#include "tooey/lexer.hpp"
#include "tooey/parser.hpp"
#include "tooey/linter.hpp"

namespace editor::services {

std::shared_ptr<tooey::AstNode> DslCompilerService::compile(const std::string& dsl) {
    try {
        auto tokens = tooey::Lexer::tokenize(dsl);
        auto ast = tooey::Parser::parse(tokens);
        return ast;
    } catch (...) {
        return nullptr;
    }
}

std::vector<LintDiagnostic> DslCompilerService::lint(const std::string& dsl) {
    std::vector<LintDiagnostic> result;
    try {
        auto diagnostics = tooey::Linter::run_diagnostics(dsl);
        result.reserve(diagnostics.size());
        for (const auto& diag : diagnostics) {
            uint32_t packed = (static_cast<uint32_t>(diag.color.r) << 24) |
                             (static_cast<uint32_t>(diag.color.g) << 16) |
                             (static_cast<uint32_t>(diag.color.b) << 8)  |
                             static_cast<uint32_t>(diag.color.a);
            result.push_back({diag.line, diag.start_col, diag.end_col, packed});
        }
    } catch (...) {
        // Safe fallback
    }
    return result;
}

} // namespace editor::services
