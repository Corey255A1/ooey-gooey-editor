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

// Custom element to draw selection outline & grips over the selected widget on the canvas
class SelectionOverlay : public gooey::mvvmc::GooeyElement {
public:
    std::string targetId;
    std::weak_ptr<gooey::mvvmc::GooeyElement> activePreview;

    SelectionOverlay() {
        is_absolute = false;
        width = {gooey::SizePolicy::MatchParent};
        height = {gooey::SizePolicy::MatchParent};
        rect_prim_ = std::make_shared<ooey::RectPrimitive>(
            ooey::Rect{0, 0, 0, 0},
            ooey::Color{0, 0, 0, 0},      // Transparent fill
            ooey::Color{0, 120, 215, 255}, // Bright blue border
            2.0f
        );
    }

    void draw(ooey::IRenderTarget& target) const override {
        auto preview = activePreview.lock();
        if (preview && !targetId.empty()) {
            auto element = find_element_by_id(preview, targetId);
            if (element && element->layout_bounds.width > 0 && element->layout_bounds.height > 0) {
                ooey::Rect outline_rect = {
                    element->layout_bounds.x - 2,
                    element->layout_bounds.y - 2,
                    element->layout_bounds.width + 4,
                    element->layout_bounds.height + 4
                };
                rect_prim_->set_rect(outline_rect);
                rect_prim_->draw(target);

                // Draw standard 6x6 grip boxes at corners
                auto r = rect_prim_->get_rect();
                ooey::Color handle_color = ooey::Color{255, 255, 255, 255};
                ooey::Color handle_border = ooey::Color{0, 120, 215, 255};
                int hs = 6;
                std::vector<ooey::Rect> handles = {
                    {r.x - hs/2, r.y - hs/2, hs, hs},
                    {r.x + r.width - hs/2, r.y - hs/2, hs, hs},
                    {r.x - hs/2, r.y + r.height - hs/2, hs, hs},
                    {r.x + r.width - hs/2, r.y + r.height - hs/2, hs, hs}
                };
                for (const auto& h : handles) {
                    ooey::RectPrimitive(h, handle_color, handle_border, 1.0f).draw(target);
                }
            }
        }
    }

private:
    static std::shared_ptr<gooey::mvvmc::GooeyElement> find_element_by_id(
        const std::shared_ptr<gooey::mvvmc::GooeyElement>& root, 
        const std::string& id) {
        if (!root) return nullptr;
        if (root->id == id) return root;
        auto node = std::dynamic_pointer_cast<gooey::mvvmc::GooeyNode>(root);
        if (node) {
            for (const auto& child : node->get_children()) {
                auto child_el = std::dynamic_pointer_cast<gooey::mvvmc::GooeyElement>(child);
                auto found = find_element_by_id(child_el, id);
                if (found) return found;
            }
        }
        return nullptr;
    }

    std::shared_ptr<ooey::RectPrimitive> rect_prim_;
};

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

    auto selectionOverlay = std::make_shared<SelectionOverlay>();
    std::shared_ptr<gooey::mvvmc::GooeyElement> activePreview;

    // Vector to keep ScopedSubscriptions alive for main()'s duration
    std::vector<gooey::mvvmc::ScopedSubscription> subs;

    // Capture dependencies weakly to prevent memory leak reference cycles
    std::weak_ptr<EditorViewModel> weak_vm = viewModel;
    std::weak_ptr<EditorView> weak_view = editorView;
    std::weak_ptr<SelectionOverlay> weak_overlay = selectionOverlay;

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
            struct HitTester {
                static std::shared_ptr<gooey::mvvmc::GooeyElement> hit_test(
                    const std::shared_ptr<gooey::mvvmc::GooeyElement>& element, 
                    int x, int y) {
                    
                    if (!element) return nullptr;
                    
                    bool hit = (x >= element->layout_bounds.x && x <= element->layout_bounds.x + element->layout_bounds.width &&
                                y >= element->layout_bounds.y && y <= element->layout_bounds.y + element->layout_bounds.height);
                    if (!hit) return nullptr;
                    
                    auto node = std::dynamic_pointer_cast<gooey::mvvmc::GooeyNode>(element);
                    if (node) {
                        for (const auto& child : node->get_children()) {
                            auto child_el = std::dynamic_pointer_cast<gooey::mvvmc::GooeyElement>(child);
                            auto child_hit = hit_test(child_el, x, y);
                            if (child_hit) return child_hit;
                        }
                    }
                    return element;
                }
            };
            
            auto hit = HitTester::hit_test(activePreview, e.x, e.y);
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

                auto tokens = tooey::Lexer::tokenize(dsl);
                
                // 1. Lexer errors (UNKNOWN tokens)
                for (const auto& tok : tokens) {
                    if (tok.type == tooey::TokenType::UNKNOWN) {
                        view->codeEditor->add_squiggle(tok.line - 1, tok.column - 1, tok.column - 1 + tok.text.size(), ooey::Color{255, 0, 0, 255});
                    }
                }

                auto ast = tooey::Parser::parse(tokens);
                if (ast) {
                    // 2. Duplicate ID check
                    std::map<std::string, const tooey::AstNode*> id_map;
                    struct DuplicateChecker {
                        gooey::controls::RichTextBox& editor;
                        std::map<std::string, const tooey::AstNode*>& id_map;

                        void check(const std::shared_ptr<tooey::AstNode>& n) {
                            if (!n) return;
                            if (!n->id.empty()) {
                                auto it = id_map.find(n->id);
                                if (it != id_map.end()) {
                                    editor.add_squiggle(n->line - 1, n->column - 1, n->column - 1 + n->id.size() + 4, ooey::Color{255, 0, 0, 255});
                                } else {
                                    id_map[n->id] = n.get();
                                }
                            }
                            for (const auto& child : n->children) check(child);
                        }
                    } dup_chk{*view->codeEditor, id_map};
                    dup_chk.check(ast);

                    // 3. Unrecognized Control types check
                    struct ElementTypeChecker {
                        gooey::controls::RichTextBox& editor;
                        void check(const std::shared_ptr<tooey::AstNode>& n) {
                            if (!n) return;
                            if (n->nodeType != "Root") {
                                static const std::set<std::string> known_types = {
                                    "VBox", "Column", "HBox", "Row", "Grid", "FlowLayout",
                                    "Button", "CheckBox", "Label", "TextBox", "RichTextBox",
                                    "ImageControl", "ScrollBar", "ScrollContainer", "ListControl",
                                    "DataGrid", "AdaptiveStack", "CanvasLayout", "VectorShapeView",
                                    "Root"
                                };
                                if (known_types.find(n->nodeType) == known_types.end()) {
                                    editor.add_squiggle(n->line - 1, n->column - 1, n->column - 1 + n->nodeType.size(), ooey::Color{255, 165, 0, 255});
                                }
                            }
                            for (const auto& child : n->children) check(child);
                        }
                    } type_chk{*view->codeEditor};
                    type_chk.check(ast);

                    // 4. Missing localization check
                    struct LocalizationChecker {
                        gooey::controls::RichTextBox& editor;
                        void check(const std::shared_ptr<tooey::AstNode>& n) {
                            if (!n) return;
                            for (const auto& prop : n->properties) {
                                if ((prop.first == "text" || prop.first == "title" || prop.first == "label") &&
                                    prop.second.type == tooey::PropertyType::String) {
                                    std::string line_text = editor.get_line_text(n->line - 1);
                                    size_t prop_pos = line_text.find(prop.first);
                                    if (prop_pos != std::string::npos) {
                                        size_t val_pos = line_text.find(prop.second.rawData, prop_pos);
                                        if (val_pos != std::string::npos) {
                                            editor.add_squiggle(n->line - 1, val_pos, val_pos + prop.second.rawData.size(), ooey::Color{255, 165, 0, 255}); // Orange warning
                                        }
                                    }
                                }
                            }
                            for (const auto& child : n->children) check(child);
                        }
                    } loc_chk{*view->codeEditor};
                    loc_chk.check(ast);

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
