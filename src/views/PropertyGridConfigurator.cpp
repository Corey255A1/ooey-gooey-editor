#include "PropertyGridConfigurator.hpp"
#include "gooey/application.hpp"
#include "gooey/mvvmc/controller.hpp"
#include "gooey/controls/label.hpp"
#include "ooey/types.hpp"

namespace editor::views {

std::shared_ptr<gooey::controls::DataGrid> PropertyGridConfigurator::create_and_configure(
    std::shared_ptr<EditorViewModel>               view_model,
    std::vector<gooey::mvvmc::ScopedSubscription>& out_subs,
    PropertyCell::FocusProvider                    focus_provider) {

    auto propertiesGrid = std::make_shared<gooey::controls::DataGrid>(
        ooey::Rect{0, 0, 200, 350}, // bounds
        35, // row height
        ooey::Font{"sans-serif", 14}
    );
    propertiesGrid->id = "propertiesGrid";
    propertiesGrid->set_absolute(false);
    propertiesGrid->set_width(gooey::SizePolicy::MatchParent);
    propertiesGrid->set_height(gooey::SizePolicy::Fixed, 350);

    PropertyCell::FocusChecker focus_checker = [](const PropertyCell* elem) {
        auto* controller = dynamic_cast<gooey::mvvmc::Controller*>(
            gooey::Application::get_instance()->get_controller());
        return controller && controller->get_focused_element().get() == static_cast<const ooey::IDrawable*>(elem);
    };

    std::vector<gooey::controls::DataGridColumn> cols;
    cols.push_back(make_property_name_column());
    cols.push_back(make_property_value_column(view_model, focus_provider, focus_checker));
    propertiesGrid->set_columns(cols);

    // Subscribe propertiesGrid to propertyItems from viewModel
    out_subs.push_back(view_model->propertyItems.subscribe([propertiesGrid](const std::vector<PropertyItem>& items) {
        std::vector<std::any> grid_items;
        for (const auto& item : items) {
            grid_items.push_back(item);
        }
        propertiesGrid->set_items(grid_items);
    }));

    return propertiesGrid;
}

gooey::controls::DataGridColumn PropertyGridConfigurator::make_property_name_column() {
    gooey::controls::DataGridColumn name_col;
    name_col.header = "Property";
    name_col.width = 90;
    name_col.cell_factory = []() {
        auto label = std::make_shared<gooey::controls::Label>();
        label->set_font(ooey::Font{"sans-serif", 14});
        label->set_color(ooey::Color{240, 240, 240});
        label->padding_left = 5;
        return label;
    };
    name_col.cell_binder = [](const std::shared_ptr<gooey::mvvmc::GooeyElement>& el, const std::any& item, int /*row_idx*/) {
        auto label = std::dynamic_pointer_cast<gooey::controls::Label>(el);
        if (label && item.has_value()) {
            auto prop = std::any_cast<PropertyItem>(item);
            label->set_text(prop.name);
        }
    };
    return name_col;
}

gooey::controls::DataGridColumn PropertyGridConfigurator::make_property_value_column(
    std::shared_ptr<EditorViewModel> view_model,
    PropertyCell::FocusProvider      focus_provider,
    PropertyCell::FocusChecker       focus_checker) {

    auto active_cells = std::make_shared<std::vector<std::weak_ptr<PropertyCell>>>();
    gooey::controls::DataGridColumn val_col;
    val_col.header = "Value";
    val_col.width = 110;
    val_col.cell_factory = [weak_vm = std::weak_ptr<EditorViewModel>(view_model), active_cells, focus_provider, focus_checker]() {
        auto cell = std::make_shared<PropertyCell>(0, weak_vm, [active_cells]() {
            for (auto& weak_c : *active_cells) {
                if (auto c = weak_c.lock()) {
                    c->set_selected(false);
                }
            }
        }, focus_provider, focus_checker);
        active_cells->push_back(cell);
        return cell;
    };
    val_col.cell_binder = [](const std::shared_ptr<gooey::mvvmc::GooeyElement>& el, const std::any& item, int row_idx) {
        auto cell = std::dynamic_pointer_cast<PropertyCell>(el);
        if (cell && item.has_value()) {
            auto prop = std::any_cast<PropertyItem>(item);
            cell->set_row_index(row_idx);
            cell->set_text(prop.value);
        }
    };
    return val_col;
}

} // namespace editor::views
