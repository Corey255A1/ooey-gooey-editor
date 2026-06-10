#include "EditorMenuBuilder.hpp"
#include "gooey/application.hpp"
#include "gooey/mvvmc/localization.hpp"
#include <iostream>

namespace editor::menus {

EditorMenuBuilder::EditorMenuBuilder(
    std::shared_ptr<EditorViewModel>          view_model,
    std::shared_ptr<gooey::ThemeManager>      theme_manager,
    std::shared_ptr<gooey::controls::MenuBar> menu_bar)
    : view_model_(view_model)
    , theme_manager_(theme_manager)
    , menu_bar_(menu_bar)
{}

void EditorMenuBuilder::initialize() {
    menu_bar_->set_categories(build());

    std::weak_ptr<EditorViewModel> weak_vm = view_model_;
    std::weak_ptr<gooey::ThemeManager> weak_tm = theme_manager_;
    std::weak_ptr<gooey::controls::MenuBar> weak_mb = menu_bar_;

    subs_.push_back(theme_manager_->active_theme.subscribe([weak_vm, weak_tm, weak_mb, this](const std::shared_ptr<gooey::Theme>&) {
        auto vm = weak_vm.lock();
        auto tm = weak_tm.lock();
        auto mb = weak_mb.lock();
        if (vm && tm && mb) {
            mb->set_categories(build());
        }
    }));

    subs_.push_back(gooey::LocalizationManager::get().active_locale.subscribe([weak_vm, weak_tm, weak_mb, this](const std::string&) {
        auto vm = weak_vm.lock();
        auto tm = weak_tm.lock();
        auto mb = weak_mb.lock();
        if (vm && tm && mb) {
            mb->set_categories(build());
        }
    }));
}

std::vector<gooey::controls::MenuCategory> EditorMenuBuilder::build() const {
    std::vector<gooey::controls::MenuCategory> cats;
    cats.push_back(build_file_menu());
    cats.push_back(build_edit_menu());
    cats.push_back(build_view_menu());
    cats.push_back(build_help_menu());
    return cats;
}

gooey::controls::MenuCategory EditorMenuBuilder::build_file_menu() const {
    auto& lm = gooey::LocalizationManager::get();
    std::weak_ptr<EditorViewModel> weak_vm = view_model_;

    gooey::controls::MenuCategory file_cat;
    file_cat.name = lm.translate("menu_file");
    file_cat.items = {
        gooey::controls::MenuItem{
            .label = lm.translate("menu_new_layout"),
            .shortcut = "Ctrl+N",
            .action = [weak_vm]() {
                if (auto vm = weak_vm.lock()) {
                    vm->dslText.set("Column id=mainLayout width=\"MatchParent\" height=\"MatchParent\":\n");
                    vm->updateCanvasFromDsl(vm->dslText.get());
                }
            }
        },
        gooey::controls::MenuItem{.is_separator = true},
        gooey::controls::MenuItem{
            .label = lm.translate("menu_exit"),
            .shortcut = "Alt+F4",
            .action = []() {
                gooey::Application::get_instance()->quit();
            }
        }
    };
    return file_cat;
}

gooey::controls::MenuCategory EditorMenuBuilder::build_view_menu() const {
    auto& lm = gooey::LocalizationManager::get();
    std::string current_theme = "dark";
    if (theme_manager_ && theme_manager_->active_theme.get()) {
        current_theme = theme_manager_->active_theme.get()->name;
    }
    std::string current_lang = lm.active_locale.get();

    std::weak_ptr<gooey::ThemeManager> weak_tm = theme_manager_;

    // View -> Language submenu
    std::vector<gooey::controls::MenuItem> lang_subitems = {
        gooey::controls::MenuItem{
            .label = lm.translate("menu_language_english"),
            .is_checkbox = true,
            .checked = (current_lang == "en_US"),
            .action = []() {
                gooey::LocalizationManager::get().active_locale.set("en_US");
            }
        },
        gooey::controls::MenuItem{
            .label = lm.translate("menu_language_spanish"),
            .is_checkbox = true,
            .checked = (current_lang == "es_ES"),
            .action = []() {
                gooey::LocalizationManager::get().active_locale.set("es_ES");
            }
        },
        gooey::controls::MenuItem{
            .label = lm.translate("menu_language_german"),
            .is_checkbox = true,
            .checked = (current_lang == "de_DE"),
            .action = []() {
                gooey::LocalizationManager::get().active_locale.set("de_DE");
            }
        },
        gooey::controls::MenuItem{
            .label = lm.translate("menu_language_french"),
            .is_checkbox = true,
            .checked = (current_lang == "fr_FR"),
            .action = []() {
                gooey::LocalizationManager::get().active_locale.set("fr_FR");
            }
        }
    };

    // View -> Theme submenu
    std::vector<gooey::controls::MenuItem> theme_subitems = {
        gooey::controls::MenuItem{
            .label = lm.translate("menu_theme_dark"),
            .is_checkbox = true,
            .checked = (current_theme == "dark"),
            .action = [weak_tm]() {
                if (auto tm = weak_tm.lock()) {
                    tm->set_active_theme("dark");
                }
            }
        },
        gooey::controls::MenuItem{
            .label = lm.translate("menu_theme_light"),
            .is_checkbox = true,
            .checked = (current_theme == "light"),
            .action = [weak_tm]() {
                if (auto tm = weak_tm.lock()) {
                    tm->set_active_theme("light");
                }
            }
        },
        gooey::controls::MenuItem{
            .label = lm.translate("menu_theme_lofi"),
            .is_checkbox = true,
            .checked = (current_theme == "lofi"),
            .action = [weak_tm]() {
                if (auto tm = weak_tm.lock()) {
                    tm->set_active_theme("lofi");
                }
            }
        },
        gooey::controls::MenuItem{
            .label = lm.translate("menu_theme_cyberpunk"),
            .is_checkbox = true,
            .checked = (current_theme == "cyberpunk"),
            .action = [weak_tm]() {
                if (auto tm = weak_tm.lock()) {
                    tm->set_active_theme("cyberpunk");
                }
            }
        }
    };

    gooey::controls::MenuCategory view_cat;
    view_cat.name = lm.translate("menu_view");
    view_cat.items = {
        gooey::controls::MenuItem{
            .label = lm.translate("menu_language"),
            .subitems = lang_subitems
        },
        gooey::controls::MenuItem{
            .label = lm.translate("menu_theme"),
            .subitems = theme_subitems
        }
    };
    return view_cat;
}

gooey::controls::MenuCategory EditorMenuBuilder::build_edit_menu() const {
    auto& lm = gooey::LocalizationManager::get();
    std::weak_ptr<EditorViewModel> weak_vm = view_model_;

    gooey::controls::MenuCategory edit_cat;
    edit_cat.name = lm.translate("menu_edit");
    edit_cat.items = {
        gooey::controls::MenuItem{
            .label = lm.translate("menu_undo"),
            .shortcut = "Ctrl+Z",
            .action = [weak_vm]() {
                if (auto vm = weak_vm.lock()) {
                    vm->undo();
                }
            }
        },
        gooey::controls::MenuItem{
            .label = lm.translate("menu_redo"),
            .shortcut = "Ctrl+Y",
            .action = [weak_vm]() {
                if (auto vm = weak_vm.lock()) {
                    vm->redo();
                }
            }
        },
        gooey::controls::MenuItem{.is_separator = true},
        gooey::controls::MenuItem{
            .label = lm.translate("menu_delete_selected"),
            .shortcut = "Delete",
            .action = [weak_vm]() {
                if (auto vm = weak_vm.lock()) {
                    vm->deleteSelectedElement();
                }
            }
        }
    };
    return edit_cat;
}

gooey::controls::MenuCategory EditorMenuBuilder::build_help_menu() const {
    auto& lm = gooey::LocalizationManager::get();

    gooey::controls::MenuCategory help_cat;
    help_cat.name = lm.translate("menu_help");
    help_cat.items = {
        gooey::controls::MenuItem{
            .label = lm.translate("menu_about"),
            .action = []() {
                std::cout << "About: OOEY-GOOEY WYSIWYG Layout Builder v1.0\n";
            }
        }
    };
    return help_cat;
}

} // namespace editor::menus
