#pragma once
#include "EditorView.hpp"
#include "viewmodels/EditorViewModel.hpp"
#include "SelectionOverlay.hpp"
#include "views/PropertyCell.hpp"
#include "gooey/mvvmc/scoped_subscription.hpp"
#include "gooey/mvvmc/theme.hpp"
#include <memory>
#include <vector>

namespace editor {

class EditorController {
public:
    EditorController(
        std::shared_ptr<EditorView>           view,
        std::shared_ptr<EditorViewModel>      view_model,
        std::shared_ptr<SelectionOverlay>     overlay,
        std::shared_ptr<gooey::ThemeManager>  theme_manager);

    void connect();

    std::vector<gooey::mvvmc::ScopedSubscription>& subscriptions() {
        return subs_;
    }

    views::PropertyCell::FocusProvider focus_provider();

private:
    void wire_toolbox_buttons();
    void wire_hierarchy_list();
    void wire_canvas_pointer();
    void wire_code_editor();
    void wire_dsl_preview_subscription();
    void sync_initial_preview();

    std::shared_ptr<EditorView>           view_;
    std::shared_ptr<EditorViewModel>      view_model_;
    std::shared_ptr<SelectionOverlay>     overlay_;
    std::shared_ptr<gooey::ThemeManager>  theme_manager_;

    std::vector<gooey::mvvmc::ScopedSubscription> subs_;
    std::shared_ptr<gooey::mvvmc::GooeyElement>   active_preview_;
};

} // namespace editor
