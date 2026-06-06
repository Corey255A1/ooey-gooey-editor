#include <iostream>
#include <memory>
#include <vector>
#include <map>
#include <set>
#include "ooey/ooey.hpp"
#include "gooey/application.hpp"
#include "ooey/platform.hpp"
#include "EditorViewModel.hpp"
#include "EditorView.hpp"
#include "DynamicInterpreter.hpp"
#include "tooey/lexer.hpp"
#include "tooey/parser.hpp"
#include "ooey/renderer/primitives/rect_primitive.hpp"
#include "gooey/mvvmc/scoped_subscription.hpp"
#include "SelectionOverlay.hpp"
#include "tooey/hit_tester.hpp"
#include "tooey/linter.hpp"

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

    auto viewModel = std::make_shared<EditorViewModel>();
    auto editorView = std::make_shared<EditorView>(viewModel);

    auto selectionOverlay = std::make_shared<editor::SelectionOverlay>();
    std::shared_ptr<gooey::mvvmc::GooeyElement> activePreview;

    // Vector to keep ScopedSubscriptions alive for main()'s duration
    std::vector<gooey::mvvmc::ScopedSubscription> subs;

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
            if (vm->is_updating_) return;
            vm->is_updating_ = true;
            vm->dslText.set(text);
            vm->updateCanvasFromDsl(text);
            vm->is_updating_ = false;
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
    editorView->set_padding(15);

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
