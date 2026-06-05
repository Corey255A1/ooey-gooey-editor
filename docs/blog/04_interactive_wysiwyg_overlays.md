# Part 4: Interactive WYSIWYG Overlays, Selections, and Manipulation

A visual editor is not complete without direct canvas interactions. Selecting, highlighting, and editing properties directly on the stage provides the intuitive feel expected from professional designers.

This post details implementing the selection overlay engine, calculating recursive layout coordinates for hit-testing, and binding canvas interactions to the Property Grid.

---

## 1. Recursive Hit-Testing on the Preview Stage

When a user clicks on the visual preview stage, the editor must identify which control lies beneath the pointer. Because layouts are nested, we must traverse the active interpreted visual tree recursively to find the topmost child enclosing the pointer coordinates.

### A. Coordinate Space Mapping
The pointer event coordinates `(e.x, e.y)` dispatched by the window backend are global to the entire application window client area. To test targets inside the preview canvas, we must transform these coordinates into canvas-local space by subtracting the absolute layout position of the `previewCanvas` itself:
```cpp
int canvas_x = e.x - editorView->previewCanvas->layout_bounds.x;
int canvas_y = e.y - editorView->previewCanvas->layout_bounds.y;
```

### B. Recursive Traversal
We implement this hit-testing logic inside the canvas pointer handler:

```cpp
editorView->previewCanvas->on_canvas_pointer = [viewModel, &activePreview, editorView](const ooey::Pointer& e) {
    if (e.state == ooey::PointerState::Pressed && activePreview) {
        int canvas_x = e.x - editorView->previewCanvas->layout_bounds.x;
        int canvas_y = e.y - editorView->previewCanvas->layout_bounds.y;

        struct HitTester {
            static std::shared_ptr<gooey::mvvmc::GooeyElement> hit_test(
                const std::shared_ptr<gooey::mvvmc::GooeyElement>& element, 
                int x, int y, int offset_x, int offset_y) {
                
                if (!element) return nullptr;

                // Accumulate absolute positions within the canvas viewport
                int local_x = element->layout_bounds.x + offset_x;
                int local_y = element->layout_bounds.y + offset_y;
                
                // Check if pointer lies within boundaries
                bool hit = (x >= local_x && x <= local_x + element->layout_bounds.width &&
                            y >= local_y && y <= local_y + element->layout_bounds.height);
                if (!hit) return nullptr;
                
                // If it is a container, test children first (topmost rendering first)
                auto node = std::dynamic_pointer_cast<gooey::mvvmc::GooeyNode>(element);
                if (node) {
                    for (const auto& child : node->get_children()) {
                        auto child_el = std::dynamic_pointer_cast<gooey::mvvmc::GooeyElement>(child);
                        auto child_hit = hit_test(child_el, x, y, local_x, local_y);
                        if (child_hit) return child_hit; // Child hit matches first
                    }
                }
                return element; // Fallback to parent container if no child matches
            }
        };
        
        auto hit = HitTester::hit_test(activePreview, canvas_x, canvas_y, 0, 0);
        if (hit && !hit->id.empty() && hit->id != "mainLayout") {
            // Synchronize selection with the hierarchy list
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
```

---

## 2. Drawing Selection Bounds and Sizing Grips (`SelectionOverlay`)

To show which widget is selected, we draw a selection indicator overlay. We implement a custom, transparent absolute-positioned control (`SelectionOverlay`) that intercepts the render cycle to draw a border and sizing grips around the selected widget:

```cpp
class SelectionOverlay : public gooey::mvvmc::GooeyElement {
public:
    std::string targetId;
    std::weak_ptr<gooey::mvvmc::GooeyElement> activePreview;

    SelectionOverlay() {
        is_absolute = true; // Renders outside normal layout constraints
        rect_prim_ = std::make_shared<ooey::RectPrimitive>(
            ooey::Rect{0, 0, 0, 0},
            ooey::Color{0, 0, 0, 0},       // Transparent background
            ooey::Color{0, 120, 215, 255},  // Bright blue highlight border
            2.0f
        );
    }

    void draw(ooey::IRenderTarget& target) const override {
        auto preview = activePreview.lock();
        if (preview && !targetId.empty()) {
            auto element = find_element_by_id(preview, targetId);
            if (element && element->layout_bounds.width > 0 && element->layout_bounds.height > 0) {
                
                // Accumulate parents' offsets to locate control within the canvas stage
                int x = 0;
                int y = 0;
                gooey::mvvmc::GooeyElement* current = element.get();
                while (current && current != preview.get()) {
                    x += current->layout_bounds.x;
                    y += current->layout_bounds.y;
                    current = current->get_parent();
                }

                // Draw bounding box slightly larger than widget
                ooey::Rect outline_rect = {
                    x - 2,
                    y - 2,
                    element->layout_bounds.width + 4,
                    element->layout_bounds.height + 4
                };
                rect_prim_->set_rect(outline_rect);
                rect_prim_->draw(target);

                // Draw 6x6 white grip boxes with blue borders at the corners
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
    std::shared_ptr<ooey::RectPrimitive> rect_prim_;

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
};
```

Whenever the selection focus changes, we call `invalidate_layout()` on the preview canvas to force a redraw pass, ensuring the selection overlay boundary recalculates and updates instantly.

---

## 3. Wiring the Property Grid to AST Mutations

When an element is selected, we query its properties from the active AST representation. In addition to raw properties, we also check for width/height properties and ensure that special values (like Localization, Data Bindings, and Signals) are properly formatted:

```cpp
void EditorViewModel::updatePropertyItems() {
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(hierarchyItems.get().size())) {
        propertyItems.set({});
        return;
    }

    auto selected_item = hierarchyItems.get()[selectedIndex];
    
    struct Finder {
        std::string targetId;
        std::shared_ptr<tooey::AstNode> find(const std::shared_ptr<tooey::AstNode>& n) {
            if (!n) return nullptr;
            if (n->id == targetId) return n;
            for (const auto& child : n->children) {
                auto found = find(child);
                if (found) return found;
            }
            return nullptr;
        }
    };

    Finder fnd{selected_item.id};
    auto node = fnd.find(currentAst);
    if (node) {
        std::vector<PropertyItem> props;
        props.push_back({"id", node->id});
        for (const auto& prop : node->properties) {
            std::string val = prop.second.rawData;
            if (prop.second.type == tooey::PropertyType::Localization) {
                val = "@tr(\"" + val + "\")";
            } else if (prop.second.type == tooey::PropertyType::Binding) {
                val = "@binding." + val;
            } else if (prop.second.type == tooey::PropertyType::Signal) {
                val = "@signal." + val;
            }
            props.push_back({prop.first, val});
        }
        
        // Ensure width and height fallbacks are present in inspector
        bool has_width = false, has_height = false;
        for (const auto& p : props) {
            if (p.name == "width") has_width = true;
            if (p.name == "height") has_height = true;
        }
        if (!has_width) props.push_back({"width", "WrapContent"});
        if (!has_height) props.push_back({"height", "WrapContent"});

        propertyItems.set(props);
    }
}
```

The property list view bindings then attach an `on_text_changed` listener to the grid textboxes:

```cpp
propVal->on_text_changed = [weak_viewModel, i](const std::string& newValue) {
    if (auto viewModel = weak_viewModel.lock()) {
        auto list_val = viewModel->propertyItems.get();
        list_val[i].value = newValue;
        viewModel->propertyItems.set(list_val); // Triggers AST mutation & re-serialization
    }
};
```

This completes the visual editing pipeline. Users can select canvas widgets, see their layout boundaries and handles, inspect details in the Property Grid, and update values in real time.

In the final post, we will critically evaluate the architectural gaps of the OOEY framework and explore future development roadmaps.
