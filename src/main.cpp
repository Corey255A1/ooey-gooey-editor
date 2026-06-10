#include <iostream>
#include <memory>
#include <vector>
#include "ooey/ooey.hpp"
#include "ooey/platform.hpp"
#include "gooey/application.hpp"
#include "viewmodels/EditorViewModel.hpp"
#include "services/HistoryManager.hpp"
#include "services/AstSerializer.hpp"
#include "services/AstMutator.hpp"
#include "services/AstQuery.hpp"
#include "services/DslCompilerService.hpp"
#include "views/PropertyGridConfigurator.hpp"
#include "controllers/EditorController.hpp"
#include "menus/EditorMenuBuilder.hpp"
#include "theme/ThemeRegistry.hpp"
#include "i18n/TranslationLoader.hpp"
#include "EditorView.hpp"
#include "SelectionOverlay.hpp"
extern "C" const char* __lsan_default_suppressions() {
    return "leak:libGLX_mesa.so\nleak:libGLX.so\nleak:libGL.so\n";
}
int main() {
    std::cout << "Starting OOEY-GOOEY WYSIWYG Editor...\n";
    gooey::Application app;
    auto backend = ooey::create_default_window_backend();
    if (!backend || !backend->create({1024, 768}, "OOEY-GOOEY Layout Builder")) {
        std::cerr << "Failed to create window\n";
        return 1;
    }
    app.set_window_backend(std::move(backend));
    auto serializer = std::make_shared<editor::services::AstSerializer>();
    auto query = std::make_shared<editor::services::AstQuery>();
    auto mutator = std::make_shared<editor::services::AstMutator>(query);
    auto compiler = std::make_shared<editor::services::DslCompilerService>();
    auto history = std::make_shared<editor::services::HistoryManager>();
    auto viewModel = std::make_shared<EditorViewModel>(mutator, serializer, query, compiler, history);
    auto editorView = std::make_shared<EditorView>(viewModel);

    auto theme_manager = editor::theme::ThemeRegistry::create_default_manager();
    app.set_theme_manager(theme_manager);
    editorView->set_theme_manager(theme_manager);
    editor::i18n::TranslationLoader::load_all();

    auto menu_builder = std::make_shared<editor::menus::EditorMenuBuilder>(viewModel, theme_manager, editorView->menuBar);
    menu_builder->initialize();
    editorView->menuBar->set_style_name("menubar");
    editorView->menuBar->set_breakpoint(600);

    auto selectionOverlay = std::make_shared<editor::SelectionOverlay>();
    auto controller = std::make_shared<editor::EditorController>(editorView, viewModel, selectionOverlay, theme_manager);
    controller->connect();

    auto propertiesGrid = editor::views::PropertyGridConfigurator::create_and_configure(
        viewModel, controller->subscriptions(), controller->focus_provider());
    editorView->rightSidebar->add_child(propertiesGrid);

    app.set_clear_color(ooey::Color{22, 22, 28});
    editorView->set_padding(0);
    app.set_root_view(editorView);
    if (std::getenv("OOEY_AUTO_QUIT")) {
        app.dispatch([&app]() { app.quit(); });
    }
    app.run();
    return 0;
}
