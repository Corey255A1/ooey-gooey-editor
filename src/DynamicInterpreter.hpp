#pragma once

#include "tooey/ast.hpp"
#include "gooey/mvvmc/gooey_element.hpp"
#include <memory>

namespace editor {

class DynamicInterpreter {
public:
    static std::shared_ptr<gooey::mvvmc::GooeyElement> interpret(const std::shared_ptr<tooey::AstNode>& node);
};

} // namespace editor
