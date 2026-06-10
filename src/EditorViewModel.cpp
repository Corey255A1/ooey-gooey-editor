#include "EditorViewModel.hpp"
#include "AstService.hpp"
#include "tooey/lexer.hpp"
#include "tooey/parser.hpp"
#include <sstream>
#include <cctype>
#include <iostream>


EditorViewModel::EditorViewModel(std::shared_ptr<editor::services::HistoryManager> history)
    : history_(history)
{
    // Populate toolbox list
    std::vector<ToolboxItem> items = {
        {"Label"}, {"Button"}, {"CheckBox"}, {"TextBox"},
        {"VBox"}, {"HBox"}, {"Grid"}, {"FlowLayout"}, {"ScrollBar"}
    };
    toolboxItems.set(items);

    // Initial default DSL layout
    std::string defaultDsl = 
        "VBox id=mainLayout spacing=20 width=\"MatchParent\" height=\"MatchParent\":\n"
        "    Label id=titleLabel text=\"Hello OOEY Visual Designer\"\n"
        "    Button id=btnClick text=\"Interact\"\n";
    
    dslText.set(defaultDsl);
    updateCanvasFromDsl(defaultDsl);

    history_->record(defaultDsl, true);

    // Subscribe to dslText changes (user typing in the editor)
    sink_.add(dslText.subscribe([this](const std::string& text) {
        if (is_updating_) {
            return;
        }
        history_->record(text, false);
    }));

    // Subscribe to property updates
    sink_.add(propertyItems.subscribe([this](const std::vector<PropertyItem>& props) {
        if (is_updating_) return;
        if (selectedIndex < 0 || selectedIndex >= static_cast<int>(hierarchyItems.get().size())) return;

        auto targetId = hierarchyItems.get()[selectedIndex].id;
        std::vector<std::pair<std::string, std::string>> properties_to_update;
        for (const auto& item : props) {
            properties_to_update.push_back({item.name, item.value});
        }

        bool changed = editor::AstService::update_node_properties(currentAst, targetId, properties_to_update);

        if (changed) {
            history_->record(dslText.get(), true);
            is_updating_ = true;
            std::string serialized = editor::AstService::serialize_ast(currentAst, 0);
            dslText.set(serialized);

            // Rebuild hierarchy and restore selection
            std::vector<HierarchyItem> h_items;
            hierarchyItems.set(h_items);
            rebuildHierarchyItems(currentAst, 0);

            const auto& new_items = hierarchyItems.get();
            for (size_t i = 0; i < new_items.size(); ++i) {
                if (new_items[i].id == targetId) {
                    selectedIndex = static_cast<int>(i);
                    break;
                }
            }
            is_updating_ = false;
        }
    }));
}

void EditorViewModel::selectElement(int index) {
    auto items = hierarchyItems.get();
    if (index >= 0 && index < static_cast<int>(items.size())) {
        selectedIndex = index;
        updatePropertyItems();
    }
}

void EditorViewModel::updateProperty(int index, const std::string& newValue) {
    if (selectedIndex < 0) return;
    auto props = propertyItems.get();
    if (index >= 0 && index < static_cast<int>(props.size())) {
        props[index].value = newValue;
        propertyItems.set(props);

        // Find the selected node in the AST and update it
        auto targetId = hierarchyItems.get()[selectedIndex].id;
        
        if (currentAst) {
            history_->record(dslText.get(), true);
            editor::AstService::update_node_properties(currentAst, targetId, {{props[index].name, newValue}});
            std::string serialized = editor::AstService::serialize_ast(currentAst, 0);
            dslText.set(serialized);
            updateCanvasFromDsl(serialized);
        }
    }
}

void EditorViewModel::addControlToCanvas(const std::string& type) {
    history_->record(dslText.get(), true);
    auto newNode = std::make_shared<tooey::AstNode>();
    newNode->nodeType = type;
    newNode->id = "new" + type + "_" + std::to_string(rand() % 1000);
    if (type == "Label") {
        newNode->properties["text"] = {tooey::PropertyType::String, "New Label"};
    } else if (type == "Button") {
        newNode->properties["text"] = {tooey::PropertyType::String, "New Button"};
    } else if (type == "CheckBox") {
        newNode->properties["text"] = {tooey::PropertyType::String, "New CheckBox"};
    }

    if (!currentAst) {
        currentAst = std::make_shared<tooey::AstNode>();
        currentAst->nodeType = "Root";
    }

    bool added = false;
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(hierarchyItems.get().size())) {
        added = editor::AstService::add_child_node(currentAst, hierarchyItems.get()[selectedIndex].id, newNode);
    }
    if (!added) {
        if (!currentAst->children.empty()) {
            currentAst->children[0]->children.push_back(newNode);
        } else {
            currentAst->children.push_back(newNode);
        }
    }

    std::string serialized = editor::AstService::serialize_ast(currentAst, 0);
    dslText.set(serialized);
    updateCanvasFromDsl(serialized);
}

void EditorViewModel::updateCanvasFromDsl(const std::string& dsl) {
    try {
        auto tokens = tooey::Lexer::tokenize(dsl);
        auto ast = tooey::Parser::parse(tokens);
        if (ast) {
            std::string selectedId = "";
            auto h_items_old = hierarchyItems.get();
            if (selectedIndex >= 0 && selectedIndex < static_cast<int>(h_items_old.size())) {
                selectedId = h_items_old[selectedIndex].id;
            }

            currentAst = ast;
            std::vector<HierarchyItem> h_items;
            hierarchyItems.set(h_items);
            rebuildHierarchyItems(currentAst, 0);

            bool restored = false;
            if (!selectedId.empty()) {
                const auto& new_items = hierarchyItems.get();
                for (size_t i = 0; i < new_items.size(); ++i) {
                    if (new_items[i].id == selectedId) {
                        selectedIndex = static_cast<int>(i);
                        updatePropertyItems();
                        restored = true;
                        break;
                    }
                }
            }

            if (!restored) {
                selectedIndex = -1;
                std::vector<PropertyItem> p_items;
                propertyItems.set(p_items);
            }
        }
    } catch (...) {
        // Safe skip on syntax check failure
    }
}

void EditorViewModel::rebuildHierarchyItems(const std::shared_ptr<tooey::AstNode>& node, int indent) {
    if (!node) return;
    if (node->nodeType != "Root") {
        auto items = hierarchyItems.get();
        items.push_back({node->nodeType, indent, node->id});
        hierarchyItems.set(items);
        for (const auto& child : node->children) {
            rebuildHierarchyItems(child, indent + 1);
        }
    } else {
        for (const auto& child : node->children) {
            rebuildHierarchyItems(child, indent);
        }
    }
}

void EditorViewModel::updatePropertyItems() {
    if (selectedIndex < 0) return;
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
        bool has_width = false, has_height = false;
        for (const auto& p : props) {
            if (p.name == "width") has_width = true;
            if (p.name == "height") has_height = true;
        }
        if (!has_width) props.push_back({"width", "WrapContent"});
        if (!has_height) props.push_back({"height", "WrapContent"});

        is_updating_ = true;
        propertyItems.set(props);
        is_updating_ = false;
    }
}

void EditorViewModel::deleteSelectedElement() {
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(hierarchyItems.get().size())) return;
    auto targetId = hierarchyItems.get()[selectedIndex].id;
    if (targetId == "mainLayout") return;

    history_->record(dslText.get(), true);

    if (editor::AstService::delete_node(currentAst, targetId)) {
        selectedIndex = -1;
        is_updating_ = true;
        std::string serialized = editor::AstService::serialize_ast(currentAst, 0);
        dslText.set(serialized);
        updateCanvasFromDsl(serialized);
        is_updating_ = false;
    }
}

void EditorViewModel::moveSelectedElementUp() {
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(hierarchyItems.get().size())) return;
    auto targetId = hierarchyItems.get()[selectedIndex].id;
    if (targetId == "mainLayout") return;

    history_->record(dslText.get(), true);

    if (editor::AstService::move_node_up(currentAst, targetId)) {
        is_updating_ = true;
        std::string serialized = editor::AstService::serialize_ast(currentAst, 0);
        dslText.set(serialized);
        updateCanvasFromDsl(serialized);
        for (size_t i = 0; i < hierarchyItems.get().size(); ++i) {
            if (hierarchyItems.get()[i].id == targetId) {
                selectedIndex = static_cast<int>(i);
                break;
            }
        }
        is_updating_ = false;
    }
}

void EditorViewModel::moveSelectedElementDown() {
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(hierarchyItems.get().size())) return;
    auto targetId = hierarchyItems.get()[selectedIndex].id;
    if (targetId == "mainLayout") return;

    history_->record(dslText.get(), true);

    if (editor::AstService::move_node_down(currentAst, targetId)) {
        is_updating_ = true;
        std::string serialized = editor::AstService::serialize_ast(currentAst, 0);
        dslText.set(serialized);
        updateCanvasFromDsl(serialized);
        for (size_t i = 0; i < hierarchyItems.get().size(); ++i) {
            if (hierarchyItems.get()[i].id == targetId) {
                selectedIndex = static_cast<int>(i);
                break;
            }
        }
        is_updating_ = false;
    }
}

void EditorViewModel::undo() {
    if (!history_->can_undo()) return;

    std::string current_state = dslText.get();
    std::string previous_state = history_->undo(current_state);

    is_updating_ = true;
    dslText.set(previous_state);
    updateCanvasFromDsl(previous_state);
    is_updating_ = false;
}

void EditorViewModel::redo() {
    if (!history_->can_redo()) return;

    std::string current_state = dslText.get();
    std::string next_state = history_->redo(current_state);

    is_updating_ = true;
    dslText.set(next_state);
    updateCanvasFromDsl(next_state);
    is_updating_ = false;
}

