#include "EditorController.hpp"
#include "DynamicInterpreter.hpp"
#include "tooey/hit_tester.hpp"
#include "tooey/linter.hpp"
#include "tooey/lexer.hpp"
#include "tooey/parser.hpp"
#include "gooey/application.hpp"
#include "gooey/mvvmc/controller.hpp"

namespace editor {

EditorController::EditorController(
    std::shared_ptr<EditorView>           view,
    std::shared_ptr<EditorViewModel>      view_model,
    std::shared_ptr<SelectionOverlay>     overlay,
    std::shared_ptr<gooey::ThemeManager>  theme_manager)
    : view_(view)
    , view_model_(view_model)
    , overlay_(overlay)
    , theme_manager_(theme_manager)
{}

void EditorController::connect() {
    wire_toolbox_buttons();
    wire_hierarchy_list();
    wire_canvas_pointer();
    wire_code_editor();
    wire_dsl_preview_subscription();
    sync_initial_preview();
}

views::PropertyCell::FocusProvider EditorController::focus_provider() {
    return [](std::shared_ptr<views::PropertyCell> elem) {
        auto* controller = dynamic_cast<gooey::mvvmc::Controller*>(
            gooey::Application::get_instance()->get_controller());
        if (controller) {
            controller->set_focused_element(elem);
        }
    };
}

void EditorController::wire_toolbox_buttons() {
    std::weak_ptr<EditorView> weak_view = view_;
    std::weak_ptr<EditorViewModel> weak_vm = view_model_;

    view_->btnAdd->on_click = [weak_vm, weak_view]() {
        auto view = weak_view.lock();
        auto vm = weak_vm.lock();
        if (view && vm) {
            int index = view->toolboxList->get_selected_index();
            if (index >= 0 && index < static_cast<int>(vm->toolboxItems.get().size())) {
                vm->addControlToCanvas(vm->toolboxItems.get()[index].name);
            }
        }
    };

    std::weak_ptr<SelectionOverlay> weak_overlay = overlay_;
    view_->btnDelete->on_click = [weak_vm, weak_view, weak_overlay]() {
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

    view_->btnMoveUp->on_click = [weak_vm, weak_view]() {
        auto view = weak_view.lock();
        auto vm = weak_vm.lock();
        if (view && vm) {
            vm->moveSelectedElementUp();
            view->hierarchyList->set_selected_index(vm->selectedIndex);
        }
    };

    view_->btnMoveDown->on_click = [weak_vm, weak_view]() {
        auto view = weak_view.lock();
        auto vm = weak_vm.lock();
        if (view && vm) {
            vm->moveSelectedElementDown();
            view->hierarchyList->set_selected_index(vm->selectedIndex);
        }
    };
}

void EditorController::wire_hierarchy_list() {
    std::weak_ptr<EditorView> weak_view = view_;
    std::weak_ptr<EditorViewModel> weak_vm = view_model_;
    std::weak_ptr<SelectionOverlay> weak_overlay = overlay_;

    view_->hierarchyList->on_selected_changed = [weak_vm, weak_overlay, weak_view](int index) {
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

    subs_.push_back(view_model_->hierarchyItems.subscribe([weak_vm, weak_view, weak_overlay](const std::vector<HierarchyItem>& /*items*/) {
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
}

void EditorController::wire_canvas_pointer() {
    std::weak_ptr<EditorView> weak_view = view_;
    std::weak_ptr<EditorViewModel> weak_vm = view_model_;

    view_->previewCanvas->on_canvas_pointer = [weak_vm, this, weak_view](const ooey::Pointer& e) {
        auto view = weak_view.lock();
        auto vm = weak_vm.lock();
        if (view && vm && e.state == ooey::PointerState::Pressed && active_preview_) {
            auto hit = tooey::HitTester::hit_test(active_preview_, e.x, e.y);
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
}

void EditorController::wire_code_editor() {
    std::weak_ptr<EditorViewModel> weak_vm = view_model_;
    std::weak_ptr<EditorView> weak_view = view_;

    view_->codeEditor->on_text_changed = [weak_vm](const std::string& text) {
        if (auto vm = weak_vm.lock()) {
            if (vm->dslText.get() == text) return;
            vm->dslText.set(text);
            vm->updateCanvasFromDsl(text);
        }
    };

    view_->codeEditor->on_undo = [weak_vm]() {
        if (auto vm = weak_vm.lock()) {
            vm->undo();
        }
    };

    view_->codeEditor->on_redo = [weak_vm]() {
        if (auto vm = weak_vm.lock()) {
            vm->redo();
        }
    };
}

void EditorController::wire_dsl_preview_subscription() {
    std::weak_ptr<EditorView> weak_view = view_;
    std::weak_ptr<EditorViewModel> weak_vm = view_model_;
    std::weak_ptr<SelectionOverlay> weak_overlay = overlay_;

    subs_.push_back(view_model_->dslText.subscribe([weak_vm, weak_view, weak_overlay, this](const std::string& dsl) {
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
                        active_preview_ = livePreview;
                        overlay->activePreview = active_preview_;
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
}

void EditorController::sync_initial_preview() {
    std::string initDsl = view_model_->dslText.get();
    try {
        view_->codeEditor->clear_squiggles();
        auto tokens = tooey::Lexer::tokenize(initDsl);
        auto ast = tooey::Parser::parse(tokens);
        if (ast) {
            auto livePreview = editor::DynamicInterpreter::interpret(ast);
            if (livePreview) {
                active_preview_ = livePreview;
                overlay_->activePreview = active_preview_;
                view_->previewCanvas->add_child(livePreview);
                view_->previewCanvas->add_child(overlay_);
            }
        }
    } catch (...) {}
}

} // namespace editor
