#include "PropertyCell.hpp"
#include "ooey/renderer/primitives/rect_primitive.hpp"
#include "ooey/renderer/primitives/line_primitive.hpp"
#include "ooey/renderer/primitives/text_primitive.hpp"

namespace editor::views {

PropertyCell::PropertyCell(
    int                          row_idx,
    std::weak_ptr<EditorViewModel> weak_vm,
    std::function<void()>          on_clear_selection,
    FocusProvider                  focus_provider,
    FocusChecker                   focus_checker)
    : row_idx_(row_idx)
    , weak_vm_(weak_vm)
    , on_clear_selection_(on_clear_selection)
    , focus_provider_(focus_provider)
    , focus_checker_(focus_checker)
{
    textbox_ = std::make_shared<gooey::controls::TextBox>();
    textbox_->set_margin(0);
}

void PropertyCell::set_row_index(int idx) {
    if (row_idx_ != idx) {
        row_idx_ = idx;
        is_editing_ = false;
    }
}

void PropertyCell::set_selected(bool selected) {
    if (is_selected_ != selected) {
        is_selected_ = selected;
        invalidate_layout();
    }
    if (!is_selected_) {
        is_editing_ = false;
    }
}

void PropertyCell::draw(ooey::IRenderTarget& target) const {
    if (is_editing_) {
        if (focus_checker_ && !focus_checker_(this)) {
            commit_change();
            is_editing_ = false;
        }
    }

    if (is_selected_) {
        ooey::RectPrimitive sel_bg(layout_bounds, selection_color_);
        sel_bg.draw(target);
    }

    if (is_editing_) {
        textbox_->draw(target);
    } else {
        std::string val = textbox_->get_text();
        ooey::Point text_pos{layout_bounds.x + 8, layout_bounds.y + (layout_bounds.height - 14) / 2};
        ooey::TextPrimitive txt(val, ooey::Font{"sans-serif", 14}, text_pos, text_color_);
        txt.draw(target);
    }

    if (is_selected_) {
        ooey::LinePrimitive top(ooey::Point{layout_bounds.x, layout_bounds.y}, ooey::Point{layout_bounds.x + layout_bounds.width, layout_bounds.y}, border_color_, 2.0f);
        ooey::LinePrimitive bottom(ooey::Point{layout_bounds.x, layout_bounds.y + layout_bounds.height}, ooey::Point{layout_bounds.x + layout_bounds.width, layout_bounds.y + layout_bounds.height}, border_color_, 2.0f);
        ooey::LinePrimitive left(ooey::Point{layout_bounds.x, layout_bounds.y}, ooey::Point{layout_bounds.x, layout_bounds.y + layout_bounds.height}, border_color_, 2.0f);
        ooey::LinePrimitive right(ooey::Point{layout_bounds.x + layout_bounds.width, layout_bounds.y}, ooey::Point{layout_bounds.x + layout_bounds.width, layout_bounds.y + layout_bounds.height}, border_color_, 2.0f);
        top.draw(target);
        bottom.draw(target);
        left.draw(target);
        right.draw(target);
    }
}

void PropertyCell::commit_change() const {
    if (auto vm = weak_vm_.lock()) {
        vm->updateProperty(row_idx_, textbox_->get_text());
    }
}

bool PropertyCell::on_pointer_event(const ooey::Pointer& e) {
    bool hit = (e.x >= layout_bounds.x && e.x <= layout_bounds.x + layout_bounds.width &&
                e.y >= layout_bounds.y && e.y <= layout_bounds.y + layout_bounds.height);

    if (hit && e.state == ooey::PointerState::Pressed) {
        if (!is_selected_) {
            if (on_clear_selection_) {
                on_clear_selection_();
            }
            is_selected_ = true;
            is_editing_ = false;
            invalidate_layout();
        } else {
            is_editing_ = true;
            original_value_ = textbox_->get_text();
            textbox_->on_pointer_event(e);
            invalidate_layout();
        }
        return true;
    }

    if (is_editing_) {
        return textbox_->on_pointer_event(e);
    }
    return false;
}

bool PropertyCell::on_key_event(const ooey::KeyEvent& e) {
    if (is_editing_) {
        if (e.state == ooey::KeyState::Pressed) {
            if (e.key_code == 0xFF1B || e.key_code == 27) { // Escape
                textbox_->set_text(original_value_);
                is_editing_ = false;
                invalidate_layout();
                if (focus_provider_) {
                    focus_provider_(shared_from_this());
                }
                return true;
            } else if (e.key_code == 0xFF0D || e.key_code == 13 || e.key_code == 10) { // Enter
                commit_change();
                is_editing_ = false;
                invalidate_layout();
                if (focus_provider_) {
                    focus_provider_(shared_from_this());
                }
                return true;
            }
        }
        return textbox_->on_key_event(e);
    }
    return false;
}

bool PropertyCell::on_text_event(const ooey::TextEvent& e) {
    if (is_editing_) {
        return textbox_->on_text_event(e);
    }
    return false;
}

void PropertyCell::set_theme_manager(std::shared_ptr<gooey::mvvmc::ThemeManager> manager) {
    GooeyElement::set_theme_manager(manager);
    if (textbox_) {
        textbox_->set_theme_manager(manager);
    }
}

void PropertyCell::apply_style(const gooey::mvvmc::Style& style) {
    text_color_ = style.text_color;
    border_color_ = style.stroke_color;
    selection_color_ = ooey::Color(border_color_.r, border_color_.g, border_color_.b, 30);
}

void PropertyCell::do_layout(ooey::Rect bounds) {
    GooeyElement::do_layout(bounds);
    if (textbox_) {
        textbox_->layout(bounds);
    }
}

} // namespace editor::views
