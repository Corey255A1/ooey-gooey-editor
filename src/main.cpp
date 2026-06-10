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
#include "controllers/EditorController.hpp"
#include "menus/EditorMenuBuilder.hpp"
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

    auto menu_builder = std::make_shared<editor::menus::EditorMenuBuilder>(
        viewModel, theme_manager, editorView->menuBar);
    menu_builder->initialize();
    editorView->menuBar->set_style_name("menubar");
    editorView->menuBar->set_breakpoint(600);

    auto selectionOverlay = std::make_shared<editor::SelectionOverlay>();

    // Controller (wires View ↔ ViewModel)
    auto controller = std::make_shared<editor::EditorController>(
        editorView, viewModel, selectionOverlay, theme_manager);
    controller->connect();

    // Property inspector grid
    auto propertiesGrid = editor::views::PropertyGridConfigurator::create_and_configure(
        viewModel, controller->subscriptions(), controller->focus_provider());
    editorView->rightSidebar->add_child(propertiesGrid);

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
