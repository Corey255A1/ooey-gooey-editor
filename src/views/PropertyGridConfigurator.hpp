#pragma once
#include "gooey/controls/datagrid.hpp"
#include "gooey/mvvmc/scoped_subscription.hpp"
#include "viewmodels/EditorViewModel.hpp"
#include "views/PropertyCell.hpp"
#include <memory>
#include <vector>

namespace editor::views {

class PropertyGridConfigurator {
public:
    // Configures columns on a new DataGrid and wires propertyItems subscription.
    static std::shared_ptr<gooey::controls::DataGrid> create_and_configure(
        std::shared_ptr<EditorViewModel>               view_model,
        std::vector<gooey::mvvmc::ScopedSubscription>& out_subs,
        PropertyCell::FocusProvider                    focus_provider);

private:
    static gooey::controls::DataGridColumn make_property_name_column();
    static gooey::controls::DataGridColumn make_property_value_column(
        std::shared_ptr<EditorViewModel> view_model,
        PropertyCell::FocusProvider      focus_provider,
        PropertyCell::FocusChecker       focus_checker);
};

} // namespace editor::views
