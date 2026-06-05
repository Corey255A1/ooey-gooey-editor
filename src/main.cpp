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
        is_absolute = true;
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
                int x = 0;
                int y = 0;
                gooey::mvvmc::GooeyElement* current = element.get();
                while (current && current != preview.get()) {
                    x += current->layout_bounds.x;
                    y += current->layout_bounds.y;
                    current = current->get_parent();
                }
                ooey::Rect outline_rect = {
                    x - 2,
                    y - 2,
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

    // Link Designer actions to buttons
    editorView->btnAdd->on_click = [viewModel, editorView]() {
        int index = editorView->toolboxList->get_selected_index();
        if (index >= 0 && index < static_cast<int>(viewModel->toolboxItems.get().size())) {
            viewModel->addControlToCanvas(viewModel->toolboxItems.get()[index].name);
        }
    };

    editorView->btnDelete->on_click = [viewModel, editorView, selectionOverlay]() {
        viewModel->deleteSelectedElement();
        selectionOverlay->targetId.clear();
        editorView->hierarchyList->set_selected_index(-1);
        editorView->previewCanvas->invalidate_layout();
    };

    editorView->btnMoveUp->on_click = [viewModel, editorView]() {
        viewModel->moveSelectedElementUp();
        editorView->hierarchyList->set_selected_index(viewModel->selectedIndex);
    };

    editorView->btnMoveDown->on_click = [viewModel, editorView]() {
        viewModel->moveSelectedElementDown();
        editorView->hierarchyList->set_selected_index(viewModel->selectedIndex);
    };

    // Link Hierarchy list selection to update property grid and selection highlight
    editorView->hierarchyList->on_selected_changed = [viewModel, selectionOverlay, editorView](int index) {
        viewModel->selectElement(index);
        if (index >= 0 && index < static_cast<int>(viewModel->hierarchyItems.get().size())) {
            selectionOverlay->targetId = viewModel->hierarchyItems.get()[index].id;
        } else {
            selectionOverlay->targetId.clear();
        }
        editorView->previewCanvas->invalidate_layout();
    };

    // Keep view list selection in sync with VM-side hierarchy items updates (e.g. from property edits or moves)
    subs.push_back(viewModel->hierarchyItems.subscribe([viewModel, editorView, selectionOverlay](const std::vector<HierarchyItem>& /*items*/) {
        if (viewModel->selectedIndex >= 0 && viewModel->selectedIndex < static_cast<int>(viewModel->hierarchyItems.get().size())) {
            editorView->hierarchyList->set_selected_index(viewModel->selectedIndex);
            selectionOverlay->targetId = viewModel->hierarchyItems.get()[viewModel->selectedIndex].id;
        } else {
            editorView->hierarchyList->set_selected_index(-1);
            selectionOverlay->targetId.clear();
        }
        editorView->previewCanvas->invalidate_layout();
    }));

    // Support clicking directly on preview canvas widgets to select them
    editorView->previewCanvas->on_canvas_pointer = [viewModel, &activePreview, editorView](const ooey::Pointer& e) {
        if (e.state == ooey::PointerState::Pressed && activePreview) {
            int canvas_x = e.x - editorView->previewCanvas->layout_bounds.x;
            int canvas_y = e.y - editorView->previewCanvas->layout_bounds.y;

            struct HitTester {
                static std::shared_ptr<gooey::mvvmc::GooeyElement> hit_test(
                    const std::shared_ptr<gooey::mvvmc::GooeyElement>& element, 
                    int x, int y, int offset_x, int offset_y) {
                    
                    if (!element) return nullptr;
                    int local_x = element->layout_bounds.x + offset_x;
                    int local_y = element->layout_bounds.y + offset_y;
                    
                    bool hit = (x >= local_x && x <= local_x + element->layout_bounds.width &&
                                y >= local_y && y <= local_y + element->layout_bounds.height);
                    if (!hit) return nullptr;
                    
                    auto node = std::dynamic_pointer_cast<gooey::mvvmc::GooeyNode>(element);
                    if (node) {
                        for (const auto& child : node->get_children()) {
                            auto child_el = std::dynamic_pointer_cast<gooey::mvvmc::GooeyElement>(child);
                            auto child_hit = hit_test(child_el, x, y, local_x, local_y);
                            if (child_hit) return child_hit;
                        }
                    }
                    return element;
                }
            };
            
            auto hit = HitTester::hit_test(activePreview, canvas_x, canvas_y, 0, 0);
            if (hit && !hit->id.empty() && hit->id != "mainLayout") {
                const auto& items = viewModel->hierarchyItems.get();
                for (size_t i = 0; i < items.size(); ++i) {
                    if (items[i].id == hit->id) {
                        viewModel->selectElement(static_cast<int>(i));
                        editorView->hierarchyList->set_selected_index(static_cast<int>(i));
                        break;
                    }
                }
            }
        }
    };

    // Propagate code editor edits back to the ViewModel reactive property
    editorView->codeEditor->on_text_changed = [viewModel](const std::string& text) {
        if (viewModel->is_updating_) return;
        viewModel->is_updating_ = true;
        viewModel->dslText.set(text);
        viewModel->updateCanvasFromDsl(text);
        viewModel->is_updating_ = false;
    };

    // Keep the live canvas preview synchronized with the DSL text edits
    subs.push_back(viewModel->dslText.subscribe([viewModel, editorView, selectionOverlay, &activePreview](const std::string& dsl) {
        try {
            editorView->codeEditor->clear_squiggles();

            auto tokens = tooey::Lexer::tokenize(dsl);
            
            // 1. Lexer errors (UNKNOWN tokens)
            for (const auto& tok : tokens) {
                if (tok.type == tooey::TokenType::UNKNOWN) {
                    editorView->codeEditor->add_squiggle(tok.line - 1, tok.column - 1, tok.column - 1 + tok.text.size(), ooey::Color{255, 0, 0, 255});
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
                } dup_chk{*editorView->codeEditor, id_map};
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
                } type_chk{*editorView->codeEditor};
                type_chk.check(ast);

                auto livePreview = editor::DynamicInterpreter::interpret(ast);
                if (livePreview) {
                    activePreview = livePreview;
                    selectionOverlay->activePreview = activePreview;
                    editorView->previewCanvas->clear_children();
                    editorView->previewCanvas->add_child(livePreview);
                    editorView->previewCanvas->add_child(selectionOverlay);
                }
            }
        } catch (...) {
            // Safe ignore syntax error during typing
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

    app.run();

    return 0;
}
