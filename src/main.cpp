#include <iostream>
#include <memory>
#include "ooey/ooey.hpp"
#include "gooey/application.hpp"
#include "ooey/platform.hpp"
#include "EditorViewModel.hpp"
#include "EditorView.hpp"
#include "DynamicInterpreter.hpp"
#include "tooey/lexer.hpp"
#include "tooey/parser.hpp"

int main() {
    std::cout << "Starting OOEY-GOOEY WYSIWYG Editor...\n";

    gooey::Application app;

    auto backend = ooey::create_default_window_backend();
    if (!backend || !backend->create({1024, 768}, "OOEY-GOOEY Layout Builder")) {
        std::cerr << "Failed to create window\n";
        return 1;
    }

    app.set_window_backend(std::move(backend));

    auto viewModel = std::make_shared<EditorViewModel>();
    auto editorView = std::make_shared<EditorView>(viewModel);

    // Link Toolbox list selection to add controls
    editorView->toolboxList->on_selected_changed = [viewModel](int index) {
        if (index >= 0 && index < static_cast<int>(viewModel->toolboxItems.get().size())) {
            viewModel->addControlToCanvas(viewModel->toolboxItems.get()[index].name);
        }
    };

    // Link Hierarchy list selection to update property grid
    editorView->hierarchyList->on_selected_changed = [viewModel](int index) {
        viewModel->selectElement(index);
    };

    // Keep the live canvas preview synchronized with the DSL text edits
    viewModel->dslText.subscribe([viewModel, editorView](const std::string& dsl) {
        try {
            auto tokens = tooey::Lexer::tokenize(dsl);
            auto ast = tooey::Parser::parse(tokens);
            if (ast) {
                auto livePreview = editor::DynamicInterpreter::interpret(ast);
                if (livePreview) {
                    editorView->previewCanvas->clear_children();
                    editorView->previewCanvas->add_child(livePreview);
                }
            }
        } catch (...) {
            // Safe ignore syntax error during typing
        }
    });

    // Run initial preview sync
    std::string initDsl = viewModel->dslText.get();
    try {
        auto tokens = tooey::Lexer::tokenize(initDsl);
        auto ast = tooey::Parser::parse(tokens);
        if (ast) {
            auto livePreview = editor::DynamicInterpreter::interpret(ast);
            if (livePreview) {
                editorView->previewCanvas->add_child(livePreview);
            }
        }
    } catch (...) {}

    // Dark layout theme
    app.set_clear_color(ooey::Color{22, 22, 28});
    editorView->set_padding(15);

    app.set_root_view(editorView);

    app.run();

    return 0;
}
