#include <iostream>
#include <memory>
#include <vector>
#include <map>
#include <set>
#include "ooey/ooey.hpp"
#include "gooey/application.hpp"
#include "ooey/platform.hpp"
#include "viewmodels/EditorViewModel.hpp"
#include "services/HistoryManager.hpp"
#include "services/AstSerializer.hpp"
#include "services/AstMutator.hpp"
#include "services/AstQuery.hpp"
#include "services/DslCompilerService.hpp"
#include "views/PropertyCell.hpp"
#include "views/PropertyGridConfigurator.hpp"
#include "EditorView.hpp"
#include "DynamicInterpreter.hpp"
#include "tooey/lexer.hpp"
#include "tooey/parser.hpp"
#include "ooey/renderer/primitives/rect_primitive.hpp"
#include "ooey/renderer/primitives/line_primitive.hpp"
#include "ooey/renderer/primitives/text_primitive.hpp"
#include "gooey/controls/datagrid.hpp"
#include "gooey/controls/label.hpp"
#include "gooey/controls/text_box.hpp"
#include "gooey/controls/menubar.hpp"
#include "gooey/controls/menu.hpp"
#include "gooey/mvvmc/theme.hpp"
#include "gooey/mvvmc/localization.hpp"
#include "gooey/mvvmc/scoped_subscription.hpp"
#include "gooey/mvvmc/controller.hpp"
#include "SelectionOverlay.hpp"
#include "tooey/hit_tester.hpp"
#include "tooey/linter.hpp"

namespace {

std::vector<gooey::controls::MenuCategory> build_editor_menu_categories(
    std::shared_ptr<EditorViewModel> viewModel,
    std::shared_ptr<gooey::ThemeManager> themeManager) {
    
    std::string current_theme = "dark";
    if (themeManager && themeManager->active_theme.get()) {
        current_theme = themeManager->active_theme.get()->name;
    }
    
    std::string current_lang = gooey::LocalizationManager::get().active_locale.get();
    auto& lm = gooey::LocalizationManager::get();

    // 1. File Menu
    gooey::controls::MenuCategory file_cat;
    file_cat.name = lm.translate("menu_file");
    file_cat.items = {
        gooey::controls::MenuItem{
            .label = lm.translate("menu_new_layout"),
            .shortcut = "Ctrl+N",
            .action = [viewModel]() {
                viewModel->dslText.set("Column id=mainLayout width=\"MatchParent\" height=\"MatchParent\":\n");
                viewModel->updateCanvasFromDsl(viewModel->dslText.get());
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

    // 2. View Menu (Language and Theme sub options)
    gooey::controls::MenuCategory view_cat;
    view_cat.name = lm.translate("menu_view");

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
            .action = [themeManager]() {
                themeManager->set_active_theme("dark");
            }
        },
        gooey::controls::MenuItem{
            .label = lm.translate("menu_theme_light"),
            .is_checkbox = true,
            .checked = (current_theme == "light"),
            .action = [themeManager]() {
                themeManager->set_active_theme("light");
            }
        },
        gooey::controls::MenuItem{
            .label = lm.translate("menu_theme_lofi"),
            .is_checkbox = true,
            .checked = (current_theme == "lofi"),
            .action = [themeManager]() {
                themeManager->set_active_theme("lofi");
            }
        },
        gooey::controls::MenuItem{
            .label = lm.translate("menu_theme_cyberpunk"),
            .is_checkbox = true,
            .checked = (current_theme == "cyberpunk"),
            .action = [themeManager]() {
                themeManager->set_active_theme("cyberpunk");
            }
        }
    };

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

    // 3. Edit Menu
    gooey::controls::MenuCategory edit_cat;
    edit_cat.name = lm.translate("menu_edit");
    edit_cat.items = {
        gooey::controls::MenuItem{
            .label = lm.translate("menu_undo"),
            .shortcut = "Ctrl+Z",
            .action = [viewModel]() {
                viewModel->undo();
            }
        },
        gooey::controls::MenuItem{
            .label = lm.translate("menu_redo"),
            .shortcut = "Ctrl+Y",
            .action = [viewModel]() {
                viewModel->redo();
            }
        },
        gooey::controls::MenuItem{.is_separator = true},
        gooey::controls::MenuItem{
            .label = lm.translate("menu_delete_selected"),
            .shortcut = "Delete",
            .action = [viewModel]() {
                viewModel->deleteSelectedElement();
            }
        }
    };

    // 4. Help Menu
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

    return {file_cat, edit_cat, view_cat, help_cat};
}
} // namespace

extern "C" const char* __lsan_default_suppressions() {
    return "leak:libGLX_mesa.so\n"
           "leak:libGLX.so\n"
           "leak:libGL.so\n";
}

int main() {
    std::cout << "Starting OOEY-GOOEY WYSIWYG Editor...\n";

    gooey::Application app;

    auto backend = ooey::create_default_window_backend();
    // Use 1024x768 to exactly fit the sum of left/right sidebars, padding, and center area
    if (!backend || !backend->create({1024, 768}, "OOEY-GOOEY Layout Builder")) {
        std::cerr << "Failed to create window\n";
        return 1;
    }

    app.set_window_backend(std::move(backend));

    std::vector<gooey::mvvmc::ScopedSubscription> subs;

    auto serializer = std::make_shared<editor::services::AstSerializer>();
    auto query = std::make_shared<editor::services::AstQuery>();
    auto mutator = std::make_shared<editor::services::AstMutator>(query);
    auto compiler = std::make_shared<editor::services::DslCompilerService>();
    auto history = std::make_shared<editor::services::HistoryManager>();
    auto viewModel = std::make_shared<EditorViewModel>(mutator, serializer, query, compiler, history);
    auto editorView = std::make_shared<EditorView>(viewModel);

    // Setup Theme Manager
    auto theme_manager = std::make_shared<gooey::ThemeManager>();

    // 1. Dark Theme
    auto dark_theme = std::make_shared<gooey::Theme>();
    dark_theme->name = "dark";
    dark_theme->set_style("window", gooey::Style{.fill_color = ooey::Color{22, 22, 28}});
    dark_theme->set_style("menubar", gooey::Style{.fill_color = ooey::Color{30, 30, 35}, .stroke_color = ooey::Color{60, 60, 70}, .text_color = ooey::Color{220, 220, 225}});
    dark_theme->set_style("menu", gooey::Style{.fill_color = ooey::Color{45, 45, 50}, .stroke_color = ooey::Color{80, 80, 90}, .text_color = ooey::Color{240, 240, 240}});
    theme_manager->add_theme("dark", dark_theme);

    // 2. Light Theme
    auto light_theme = std::make_shared<gooey::Theme>();
    light_theme->name = "light";
    light_theme->set_style("window", gooey::Style{.fill_color = ooey::Color{240, 240, 245}});
    light_theme->set_style("menubar", gooey::Style{.fill_color = ooey::Color{225, 225, 230}, .stroke_color = ooey::Color{180, 180, 190}, .text_color = ooey::Color{40, 40, 45}});
    light_theme->set_style("menu", gooey::Style{.fill_color = ooey::Color{255, 255, 255}, .stroke_color = ooey::Color{180, 180, 190}, .text_color = ooey::Color{40, 40, 45}});
    theme_manager->add_theme("light", light_theme);

    // 3. Lofi Theme
    auto lofi_theme = std::make_shared<gooey::Theme>();
    lofi_theme->name = "lofi";
    lofi_theme->set_style("window", gooey::Style{.fill_color = ooey::Color{50, 45, 45}});
    lofi_theme->set_style("menubar", gooey::Style{.fill_color = ooey::Color{65, 60, 60}, .stroke_color = ooey::Color{90, 80, 80}, .text_color = ooey::Color{210, 200, 200}});
    lofi_theme->set_style("menu", gooey::Style{.fill_color = ooey::Color{70, 65, 65}, .stroke_color = ooey::Color{110, 100, 100}, .text_color = ooey::Color{230, 220, 220}});
    theme_manager->add_theme("lofi", lofi_theme);

    // 4. Cyberpunk Theme
    auto cyberpunk_theme = std::make_shared<gooey::Theme>();
    cyberpunk_theme->name = "cyberpunk";
    cyberpunk_theme->set_style("window", gooey::Style{.fill_color = ooey::Color{10, 10, 15}});
    cyberpunk_theme->set_style("menubar", gooey::Style{.fill_color = ooey::Color{0, 0, 0}, .stroke_color = ooey::Color{255, 0, 128}, .text_color = ooey::Color{0, 255, 255}});
    cyberpunk_theme->set_style("menu", gooey::Style{.fill_color = ooey::Color{15, 15, 20}, .stroke_color = ooey::Color{255, 0, 128}, .text_color = ooey::Color{0, 255, 255}});
    theme_manager->add_theme("cyberpunk", cyberpunk_theme);

    theme_manager->set_active_theme("dark");
    app.set_theme_manager(theme_manager);
    editorView->set_theme_manager(theme_manager);

    // Load translation dictionaries for supported locales.
    std::map<std::string, std::string> en_dict = {
        {"menu_file", "File"},
        {"menu_new_layout", "New Layout"},
        {"menu_exit", "Exit"},
        {"menu_view", "View"},
        {"menu_language", "Language"},
        {"menu_language_english", "English"},
        {"menu_language_spanish", "Spanish"},
        {"menu_language_german", "German"},
        {"menu_language_french", "French"},
        {"menu_theme", "Theme"},
        {"menu_theme_dark", "Dark"},
        {"menu_theme_light", "Light"},
        {"menu_theme_lofi", "Lofi"},
        {"menu_theme_cyberpunk", "Cyberpunk"},
        {"menu_edit", "Edit"},
        {"menu_undo", "Undo"},
        {"menu_redo", "Redo"},
        {"menu_delete_selected", "Delete Selected"},
        {"menu_help", "Help"},
        {"menu_about", "About"},
        {"tool_palette", "Tool Palette"},
        {"add_selected", "Add Selected"},
        {"tree_hierarchy", "Tree Hierarchy"},
        {"delete", "Delete"},
        {"up", "Up"},
        {"down", "Down"},
        {"live_preview", "Live Preview"},
        {"dsl_editor", "DSL Editor"},
        {"property_inspector", "Property Inspector"}
    };
    std::map<std::string, std::string> es_dict = {
        {"menu_file", "Archivo"},
        {"menu_new_layout", "Nuevo diseño"},
        {"menu_exit", "Salir"},
        {"menu_view", "Ver"},
        {"menu_language", "Idioma"},
        {"menu_language_english", "Inglés"},
        {"menu_language_spanish", "Español"},
        {"menu_language_german", "Alemán"},
        {"menu_language_french", "Francés"},
        {"menu_theme", "Tema"},
        {"menu_theme_dark", "Oscuro"},
        {"menu_theme_light", "Claro"},
        {"menu_theme_lofi", "Lofi"},
        {"menu_theme_cyberpunk", "Ciberpunk"},
        {"menu_edit", "Editar"},
        {"menu_undo", "Deshacer"},
        {"menu_redo", "Rehacer"},
        {"menu_delete_selected", "Eliminar seleccionado"},
        {"menu_help", "Ayuda"},
        {"menu_about", "Acerca de"},
        {"tool_palette", "Paleta de herramientas"},
        {"add_selected", "Agregar seleccionado"},
        {"tree_hierarchy", "Jerarquía"},
        {"delete", "Eliminar"},
        {"up", "Arriba"},
        {"down", "Abajo"},
        {"live_preview", "Vista previa en vivo"},
        {"dsl_editor", "Editor DSL"},
        {"property_inspector", "Inspector de propiedades"}
    };
    std::map<std::string, std::string> de_dict = {
        {"menu_file", "Datei"},
        {"menu_new_layout", "Neues Layout"},
        {"menu_exit", "Beenden"},
        {"menu_view", "Ansicht"},
        {"menu_language", "Sprache"},
        {"menu_language_english", "Englisch"},
        {"menu_language_spanish", "Spanisch"},
        {"menu_language_german", "Deutsch"},
        {"menu_language_french", "Französisch"},
        {"menu_theme", "Thema"},
        {"menu_theme_dark", "Dunkel"},
        {"menu_theme_light", "Hell"},
        {"menu_theme_lofi", "Lofi"},
        {"menu_theme_cyberpunk", "Cyberpunk"},
        {"menu_edit", "Bearbeiten"},
        {"menu_undo", "Rückgängig"},
        {"menu_redo", "Wiederholen"},
        {"menu_delete_selected", "Ausgewählte löschen"},
        {"menu_help", "Hilfe"},
        {"menu_about", "Über"},
        {"tool_palette", "Werkzeugpalette"},
        {"add_selected", "Ausgewählt hinzufügen"},
        {"tree_hierarchy", "Baumansicht"},
        {"delete", "Löschen"},
        {"up", "Oben"},
        {"down", "Unten"},
        {"live_preview", "Live-Vorschau"},
        {"dsl_editor", "DSL-Editor"},
        {"property_inspector", "Eigenschaftsinspektor"}
    };
    std::map<std::string, std::string> fr_dict = {
        {"menu_file", "Fichier"},
        {"menu_new_layout", "Nouveau layout"},
        {"menu_exit", "Quitter"},
        {"menu_view", "Affichage"},
        {"menu_language", "Langue"},
        {"menu_language_english", "Anglais"},
        {"menu_language_spanish", "Espagnol"},
        {"menu_language_german", "Allemand"},
        {"menu_language_french", "Français"},
        {"menu_theme", "Thème"},
        {"menu_theme_dark", "Sombre"},
        {"menu_theme_light", "Clair"},
        {"menu_theme_lofi", "Lofi"},
        {"menu_theme_cyberpunk", "Cyberpunk"},
        {"menu_edit", "Modifier"},
        {"menu_undo", "Annuler"},
        {"menu_redo", "Rétablir"},
        {"menu_delete_selected", "Supprimer la sélection"},
        {"menu_help", "Aide"},
        {"menu_about", "À propos"},
        {"tool_palette", "Palette d'outils"},
        {"add_selected", "Ajouter la sélection"},
        {"tree_hierarchy", "Hiérarchie"},
        {"delete", "Supprimer"},
        {"up", "Haut"},
        {"down", "Bas"},
        {"live_preview", "Aperçu en direct"},
        {"dsl_editor", "Éditeur DSL"},
        {"property_inspector", "Inspecteur de propriété"}
    };
    gooey::LocalizationManager::get().load_translations("en_US", en_dict);
    gooey::LocalizationManager::get().load_translations("es_ES", es_dict);
    gooey::LocalizationManager::get().load_translations("de_DE", de_dict);
    gooey::LocalizationManager::get().load_translations("fr_FR", fr_dict);

    editorView->menuBar->set_categories(build_editor_menu_categories(viewModel, theme_manager));
    editorView->menuBar->set_style_name("menubar");
    editorView->menuBar->set_breakpoint(600);

    // Subscribe to theme and locale changes to rebuild menu categories dynamically
    std::weak_ptr<EditorView> weak_view_menu = editorView;
    std::weak_ptr<EditorViewModel> weak_vm_menu = viewModel;
    std::weak_ptr<gooey::ThemeManager> weak_theme_manager = theme_manager;

    subs.push_back(theme_manager->active_theme.subscribe([weak_view_menu, weak_vm_menu, weak_theme_manager](const std::shared_ptr<gooey::Theme>&) {
        auto view = weak_view_menu.lock();
        auto vm = weak_vm_menu.lock();
        auto tm = weak_theme_manager.lock();
        if (view && vm && tm) {
            view->menuBar->set_categories(build_editor_menu_categories(vm, tm));
        }
    }));

    subs.push_back(gooey::LocalizationManager::get().active_locale.subscribe([weak_view_menu, weak_vm_menu, weak_theme_manager](const std::string&) {
        auto view = weak_view_menu.lock();
        auto vm = weak_vm_menu.lock();
        auto tm = weak_theme_manager.lock();
        if (view && vm && tm) {
            view->menuBar->set_categories(build_editor_menu_categories(vm, tm));
        }
    }));

    editor::views::PropertyCell::FocusProvider focus_provider = [](std::shared_ptr<editor::views::PropertyCell> elem) {
        auto* controller = dynamic_cast<gooey::mvvmc::Controller*>(
            gooey::Application::get_instance()->get_controller());
        if (controller) {
            controller->set_focused_element(elem);
        }
    };

    auto propertiesGrid = editor::views::PropertyGridConfigurator::create_and_configure(
        viewModel, subs, focus_provider);
    editorView->rightSidebar->add_child(propertiesGrid);

    auto selectionOverlay = std::make_shared<editor::SelectionOverlay>();
    std::shared_ptr<gooey::mvvmc::GooeyElement> activePreview;

    // Capture dependencies weakly to prevent memory leak reference cycles
    std::weak_ptr<EditorViewModel> weak_vm = viewModel;
    std::weak_ptr<EditorView> weak_view = editorView;
    std::weak_ptr<editor::SelectionOverlay> weak_overlay = selectionOverlay;

    // Link Designer actions to buttons
    editorView->btnAdd->on_click = [weak_vm, weak_view]() {
        auto view = weak_view.lock();
        auto vm = weak_vm.lock();
        if (view && vm) {
            int index = view->toolboxList->get_selected_index();
            if (index >= 0 && index < static_cast<int>(vm->toolboxItems.get().size())) {
                vm->addControlToCanvas(vm->toolboxItems.get()[index].name);
            }
        }
    };

    editorView->btnDelete->on_click = [weak_vm, weak_view, weak_overlay]() {
        auto view = weak_view.lock();
        auto vm = weak_vm.lock();
        auto overlay = weak_overlay.lock();
        if (view && vm && overlay) {
            vm->deleteSelectedElement();
            overlay->targetId.clear();
            view->hierarchyList->set_selected_index(-1);
            view->previewCanvas->invalidate_layout();
        }
    };

    editorView->btnMoveUp->on_click = [weak_vm, weak_view]() {
        auto view = weak_view.lock();
        auto vm = weak_vm.lock();
        if (view && vm) {
            vm->moveSelectedElementUp();
            view->hierarchyList->set_selected_index(vm->selectedIndex);
        }
    };

    editorView->btnMoveDown->on_click = [weak_vm, weak_view]() {
        auto view = weak_view.lock();
        auto vm = weak_vm.lock();
        if (view && vm) {
            vm->moveSelectedElementDown();
            view->hierarchyList->set_selected_index(vm->selectedIndex);
        }
    };

    // Link Hierarchy list selection to update property grid and selection highlight
    editorView->hierarchyList->on_selected_changed = [weak_vm, weak_overlay, weak_view](int index) {
        auto view = weak_view.lock();
        auto vm = weak_vm.lock();
        auto overlay = weak_overlay.lock();
        if (view && vm && overlay) {
            vm->selectElement(index);
            if (index >= 0 && index < static_cast<int>(vm->hierarchyItems.get().size())) {
                overlay->targetId = vm->hierarchyItems.get()[index].id;
            } else {
                overlay->targetId.clear();
            }
            view->previewCanvas->invalidate_layout();
        }
    };

    // Keep view list selection in sync with VM-side hierarchy items updates (e.g. from property edits or moves)
    subs.push_back(viewModel->hierarchyItems.subscribe([weak_vm, weak_view, weak_overlay](const std::vector<HierarchyItem>& /*items*/) {
        auto view = weak_view.lock();
        auto vm = weak_vm.lock();
        auto overlay = weak_overlay.lock();
        if (view && vm && overlay) {
            if (vm->selectedIndex >= 0 && vm->selectedIndex < static_cast<int>(vm->hierarchyItems.get().size())) {
                view->hierarchyList->set_selected_index(vm->selectedIndex);
                overlay->targetId = vm->hierarchyItems.get()[vm->selectedIndex].id;
            } else {
                view->hierarchyList->set_selected_index(-1);
                overlay->targetId.clear();
            }
            view->previewCanvas->invalidate_layout();
        }
    }));

    // Support clicking directly on preview canvas widgets to select them
    editorView->previewCanvas->on_canvas_pointer = [weak_vm, &activePreview, weak_view](const ooey::Pointer& e) {
        auto view = weak_view.lock();
        auto vm = weak_vm.lock();
        if (view && vm && e.state == ooey::PointerState::Pressed && activePreview) {
            auto hit = tooey::HitTester::hit_test(activePreview, e.x, e.y);
            if (hit && !hit->id.empty() && hit->id != "mainLayout") {
                const auto& items = vm->hierarchyItems.get();
                for (size_t i = 0; i < items.size(); ++i) {
                    if (items[i].id == hit->id) {
                        vm->selectElement(static_cast<int>(i));
                        view->hierarchyList->set_selected_index(static_cast<int>(i));
                        break;
                    }
                }
            }
        }
    };

    // Propagate code editor edits back to the ViewModel reactive property
    editorView->codeEditor->on_text_changed = [weak_vm](const std::string& text) {
        if (auto vm = weak_vm.lock()) {
            if (vm->dslText.get() == text) return;
            vm->dslText.set(text);
            vm->updateCanvasFromDsl(text);
        }
    };

    editorView->codeEditor->on_undo = [weak_vm]() {
        if (auto vm = weak_vm.lock()) {
            vm->undo();
        }
    };

    editorView->codeEditor->on_redo = [weak_vm]() {
        if (auto vm = weak_vm.lock()) {
            vm->redo();
        }
    };

    // Keep the live canvas preview synchronized with the DSL text edits
    subs.push_back(viewModel->dslText.subscribe([weak_vm, weak_view, weak_overlay, &activePreview](const std::string& dsl) {
        auto view = weak_view.lock();
        auto vm = weak_vm.lock();
        auto overlay = weak_overlay.lock();
        if (view && vm && overlay) {
            try {
                view->codeEditor->clear_squiggles();

                auto diagnostics = tooey::Linter::run_diagnostics(dsl);
                for (const auto& diag : diagnostics) {
                    view->codeEditor->add_squiggle(diag.line, diag.start_col, diag.end_col, diag.color);
                }

                auto tokens = tooey::Lexer::tokenize(dsl);
                auto ast = tooey::Parser::parse(tokens);
                if (ast) {
                    auto livePreview = editor::DynamicInterpreter::interpret(ast);
                    if (livePreview) {
                        activePreview = livePreview;
                        overlay->activePreview = activePreview;
                        view->previewCanvas->clear_children();
                        view->previewCanvas->add_child(livePreview);
                        view->previewCanvas->add_child(overlay);
                    }
                }
            } catch (...) {
                // Safe ignore syntax error during typing
            }
        }
    }));

    // Run initial preview sync
    std::string initDsl = viewModel->dslText.get();
    try {
        editorView->codeEditor->clear_squiggles();
        auto tokens = tooey::Lexer::tokenize(initDsl);
        auto ast = tooey::Parser::parse(tokens);
        if (ast) {
            auto livePreview = editor::DynamicInterpreter::interpret(ast);
            if (livePreview) {
                activePreview = livePreview;
                selectionOverlay->activePreview = activePreview;
                editorView->previewCanvas->add_child(livePreview);
                editorView->previewCanvas->add_child(selectionOverlay);
            }
        }
    } catch (...) {}

    // Dark layout theme
    app.set_clear_color(ooey::Color{22, 22, 28});
    editorView->set_padding(0);

    app.set_root_view(editorView);

    const char* auto_quit = std::getenv("OOEY_AUTO_QUIT");
    if (auto_quit) {
        app.dispatch([&app]() {
            app.quit();
        });
    }



    app.run();

    return 0;
}
