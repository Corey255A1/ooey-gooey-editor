#pragma once
#include "gooey/controls/menubar.hpp"
#include "gooey/mvvmc/theme.hpp"
#include "viewmodels/EditorViewModel.hpp"
#include "gooey/mvvmc/scoped_subscription.hpp"
#include <memory>
#include <vector>

namespace editor::menus {

class EditorMenuBuilder {
public:
    EditorMenuBuilder(
        std::shared_ptr<EditorViewModel>          view_model,
        std::shared_ptr<gooey::ThemeManager>      theme_manager,
        std::shared_ptr<gooey::controls::MenuBar> menu_bar);

    void initialize();

private:
    std::vector<gooey::controls::MenuCategory> build() const;

    gooey::controls::MenuCategory build_file_menu() const;
    gooey::controls::MenuCategory build_edit_menu() const;
    gooey::controls::MenuCategory build_view_menu() const;
    gooey::controls::MenuCategory build_help_menu() const;

    std::shared_ptr<EditorViewModel>          view_model_;
    std::shared_ptr<gooey::ThemeManager>      theme_manager_;
    std::shared_ptr<gooey::controls::MenuBar> menu_bar_;

    std::vector<gooey::mvvmc::ScopedSubscription> subs_;
};

} // namespace editor::menus
