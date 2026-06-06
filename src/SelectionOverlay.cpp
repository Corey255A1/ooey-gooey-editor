#include "SelectionOverlay.hpp"
#include "gooey/mvvmc/gooey_node.hpp"
#include "ooey/renderer/primitives/rect_primitive.hpp"
#include <vector>

namespace editor {

SelectionOverlay::SelectionOverlay() {
    is_absolute = false;
    width = {gooey::SizePolicy::MatchParent};
    height = {gooey::SizePolicy::MatchParent};
    rect_prim_ = std::make_shared<ooey::RectPrimitive>(
        ooey::Rect{0, 0, 0, 0},
        ooey::Color{0, 0, 0, 0},      // Transparent fill
        ooey::Color{0, 120, 215, 255}, // Bright blue border
        2.0f
    );
}

void SelectionOverlay::draw(ooey::IRenderTarget& target) const {
    auto preview = activePreview.lock();
    if (preview && !targetId.empty()) {
        auto element = find_element_by_id(preview, targetId);
        if (element && element->layout_bounds.width > 0 && element->layout_bounds.height > 0) {
            ooey::Rect outline_rect = {
                element->layout_bounds.x - 2,
                element->layout_bounds.y - 2,
                element->layout_bounds.width + 4,
                element->layout_bounds.height + 4
            };
            rect_prim_->set_rect(outline_rect);
            rect_prim_->draw(target);

            // Draw standard 6x6 grip boxes at corners
            auto r = rect_prim_->get_rect();
            ooey::Color handle_color = ooey::Color{255, 255, 255, 255};
            ooey::Color handle_border = ooey::Color{0, 120, 215, 255};
            int hs = 6;
            std::vector<ooey::Rect> handles = {
                {r.x - hs/2, r.y - hs/2, hs, hs},
                {r.x + r.width - hs/2, r.y - hs/2, hs, hs},
                {r.x - hs/2, r.y + r.height - hs/2, hs, hs},
                {r.x + r.width - hs/2, r.y + r.height - hs/2, hs, hs}
            };
            for (const auto& h : handles) {
                ooey::RectPrimitive(h, handle_color, handle_border, 1.0f).draw(target);
            }
        }
    }
}

std::shared_ptr<gooey::mvvmc::GooeyElement> SelectionOverlay::find_element_by_id(
    const std::shared_ptr<gooey::mvvmc::GooeyElement>& root, 
    const std::string& id) {
    if (!root) return nullptr;
    if (root->id == id) return root;
    auto node = std::dynamic_pointer_cast<gooey::mvvmc::GooeyNode>(root);
    if (node) {
        for (const auto& child : node->get_children()) {
            auto child_el = std::dynamic_pointer_cast<gooey::mvvmc::GooeyElement>(child);
            auto found = find_element_by_id(child_el, id);
            if (found) return found;
        }
    }
    return nullptr;
}

} // namespace editor
