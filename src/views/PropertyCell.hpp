#pragma once
#include "gooey/mvvmc/gooey_element.hpp"
#include "gooey/mvvmc/i_interactive.hpp"
#include "gooey/mvvmc/theme.hpp"
#include "gooey/controls/text_box.hpp"
#include "viewmodels/EditorViewModel.hpp"
#include <memory>
#include <functional>
#include <string>

namespace editor::views {

class PropertyCell
    : public gooey::GooeyElement
    , public gooey::mvvmc::IInteractive
    , public std::enable_shared_from_this<PropertyCell>
{
public:
    using FocusProvider = std::function<void(std::shared_ptr<PropertyCell>)>;
    using FocusChecker = std::function<bool(const PropertyCell*)>;

    PropertyCell(
        int                          row_idx,
        std::weak_ptr<EditorViewModel> weak_vm,
        std::function<void()>          on_clear_selection,
        FocusProvider                  focus_provider,
        FocusChecker                   focus_checker);

    ooey::Rect bounds() const override { return layout_bounds; }

    void set_text(const std::string& text) { textbox_->set_text(text); }
    std::string get_text() const { return textbox_->get_text(); }
    void set_row_index(int idx);

    void set_selected(bool selected);
    bool is_selected() const { return is_selected_; }

    void draw(ooey::IRenderTarget& target) const override;
    void commit_change() const;

    bool on_pointer_event(const ooey::Pointer& e) override;
    bool on_key_event(const ooey::KeyEvent& e) override;
    bool on_text_event(const ooey::TextEvent& e) override;

    void set_theme_manager(std::shared_ptr<gooey::mvvmc::ThemeManager> manager) override;
    void apply_style(const gooey::mvvmc::Style& style) override;

    std::shared_ptr<gooey::controls::TextBox> textbox() const {
        return textbox_;
    }

protected:
    void do_layout(ooey::Rect bounds) override;

private:
    std::shared_ptr<gooey::controls::TextBox> textbox_;
    int row_idx_;
    std::weak_ptr<EditorViewModel> weak_vm_;
    std::function<void()> on_clear_selection_;
    FocusProvider focus_provider_;
    FocusChecker focus_checker_;

    bool is_selected_{false};
    mutable bool is_editing_{false};
    std::string original_value_;

    ooey::Color text_color_{240, 240, 240};
    ooey::Color selection_color_{0, 120, 215, 30};
    ooey::Color border_color_{0, 120, 215};
};

} // namespace editor::views
