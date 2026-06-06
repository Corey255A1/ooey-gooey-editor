#pragma once

#include "gooey/mvvmc/gooey_element.hpp"
#include "ooey/renderer/primitives/rect_primitive.hpp"
#include <memory>
#include <string>

namespace editor {

class SelectionOverlay : public gooey::mvvmc::GooeyElement {
public:
    std::string targetId;
    std::weak_ptr<gooey::mvvmc::GooeyElement> activePreview;

    SelectionOverlay();

    void draw(ooey::IRenderTarget& target) const override;

private:
    static std::shared_ptr<gooey::mvvmc::GooeyElement> find_element_by_id(
        const std::shared_ptr<gooey::mvvmc::GooeyElement>& root, 
        const std::string& id);

    std::shared_ptr<ooey::RectPrimitive> rect_prim_;
};

} // namespace editor
