#include "EditorViewModel.hpp"
#include "tooey/lexer.hpp"
#include "tooey/parser.hpp"
#include <sstream>
#include <cctype>


EditorViewModel::EditorViewModel() {
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

    last_dsl_ = defaultDsl;
    last_typing_record_time_ = std::chrono::steady_clock::now();

    // Subscribe to dslText changes (user typing in the editor)
    sink_.add(dslText.subscribe([this](const std::string& text) {
        if (is_updating_) {
            last_dsl_ = text;
            return;
        }
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_typing_record_time_).count();
        if (elapsed > 1000 || last_dsl_.empty()) {
            if (undo_stack_.empty() || undo_stack_.back() != last_dsl_) {
                undo_stack_.push_back(last_dsl_);
                redo_stack_.clear();
                if (undo_stack_.size() > 100) {
                    undo_stack_.erase(undo_stack_.begin());
                }
            }
            last_typing_record_time_ = now;
        }
        last_dsl_ = text;
    }));

    // Subscribe to property updates
    sink_.add(propertyItems.subscribe([this](const std::vector<PropertyItem>& props) {
        if (is_updating_) return;
        if (selectedIndex < 0 || selectedIndex >= static_cast<int>(hierarchyItems.get().size())) return;

        auto targetId = hierarchyItems.get()[selectedIndex].id;
        bool changed = false;

        struct NodeUpdater {
            std::string targetId;
            const std::vector<PropertyItem>& props;
            bool& changed;

            void update(const std::shared_ptr<tooey::AstNode>& n) {
                if (!n) return;
                if (n->id == targetId) {
                    for (const auto& item : props) {
                        if (item.name == "id") {
                            if (n->id != item.value) {
                                n->id = item.value;
                                changed = true;
                            }
                        } else {
                            tooey::PropertyValue new_val;
                            std::string val = item.value;
                            if (val == "true" || val == "false") {
                                new_val.type = tooey::PropertyType::Boolean;
                            } else if (val.starts_with("@binding.")) {
                                new_val.type = tooey::PropertyType::Binding;
                                val = val.substr(9);
                            } else if (val.starts_with("@signal.")) {
                                new_val.type = tooey::PropertyType::Signal;
                                val = val.substr(8);
                            } else if (val.starts_with("@tr(")) {
                                new_val.type = tooey::PropertyType::Localization;
                                size_t first = val.find("\"");
                                size_t last = val.rfind("\"");
                                if (first != std::string::npos && last != std::string::npos && last > first) {
                                    val = val.substr(first + 1, last - first - 1);
                                }
                            } else {
                                bool is_num = true;
                                for (char c : val) {
                                    if (!std::isdigit(c)) { is_num = false; break; }
                                }
                                new_val.type = is_num ? tooey::PropertyType::Number : tooey::PropertyType::String;
                            }
                            new_val.rawData = val;

                            auto it = n->properties.find(item.name);
                            if (it == n->properties.end() || it->second.rawData != new_val.rawData || it->second.type != new_val.type) {
                                n->properties[item.name] = new_val;
                                changed = true;
                            }
                        }
                    }
                }
                for (const auto& child : n->children) {
                    update(child);
                }
            }
        };

        NodeUpdater upd{targetId, props, changed};
        upd.update(currentAst);

        if (changed) {
            record_history();
            is_updating_ = true;
            std::string serialized = generateDslFromAst(currentAst, 0);
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
        
        struct Updater {
            std::string targetId;
            std::string prop_name;
            std::string newValue;
            
            bool update(const std::shared_ptr<tooey::AstNode>& n) {
                if (!n) return false;
                if (n->id == targetId) {
                    tooey::PropertyValue val;
                    if (newValue == "true" || newValue == "false") {
                        val.type = tooey::PropertyType::Boolean;
                    } else if (newValue.starts_with("@binding.")) {
                        val.type = tooey::PropertyType::Binding;
                        val.rawData = newValue.substr(9);
                    } else if (newValue.starts_with("@signal.")) {
                        val.type = tooey::PropertyType::Signal;
                        val.rawData = newValue.substr(8);
                    } else if (newValue.starts_with("@tr(")) {
                        val.type = tooey::PropertyType::Localization;
                        size_t first = newValue.find("\"");
                        size_t last = newValue.rfind("\"");
                        if (first != std::string::npos && last != std::string::npos && last > first) {
                            val.rawData = newValue.substr(first + 1, last - first - 1);
                        } else {
                            val.rawData = newValue;
                        }
                    } else {
                        bool is_num = true;
                        for (char c : newValue) {
                            if (!std::isdigit(c)) { is_num = false; break; }
                        }
                        val.type = is_num ? tooey::PropertyType::Number : tooey::PropertyType::String;
                        val.rawData = newValue;
                    }
                    if (prop_name == "id") {
                        n->id = newValue;
                    } else {
                        n->properties[prop_name] = val;
                    }
                    return true;
                }
                for (const auto& child : n->children) {
                    if (update(child)) return true;
                }
                return false;
            }
        };

        if (currentAst) {
            record_history();
            Updater upd{targetId, props[index].name, newValue};
            upd.update(currentAst);
            std::string serialized = generateDslFromAst(currentAst, 0);
            dslText.set(serialized);
            updateCanvasFromDsl(serialized);
        }
    }
}

void EditorViewModel::addControlToCanvas(const std::string& type) {
    record_history();
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

    struct Insertion {
        std::string targetId;
        std::shared_ptr<tooey::AstNode> newNode;
        
        bool add(const std::shared_ptr<tooey::AstNode>& n) {
            if (!n) return false;
            if (n->id == targetId) {
                n->children.push_back(newNode);
                return true;
            }
            for (const auto& child : n->children) {
                if (add(child)) return true;
            }
            return false;
        }
    };

    bool added = false;
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(hierarchyItems.get().size())) {
        Insertion ins{hierarchyItems.get()[selectedIndex].id, newNode};
        added = ins.add(currentAst);
    }
    if (!added) {
        if (!currentAst->children.empty()) {
            currentAst->children[0]->children.push_back(newNode);
        } else {
            currentAst->children.push_back(newNode);
        }
    }

    std::string serialized = generateDslFromAst(currentAst, 0);
    dslText.set(serialized);
    updateCanvasFromDsl(serialized);
}

void EditorViewModel::updateCanvasFromDsl(const std::string& dsl) {
    try {
        auto tokens = tooey::Lexer::tokenize(dsl);
        auto ast = tooey::Parser::parse(tokens);
        if (ast) {
            currentAst = ast;
            std::vector<HierarchyItem> h_items;
            hierarchyItems.set(h_items);
            rebuildHierarchyItems(currentAst, 0);
            selectedIndex = -1;
            std::vector<PropertyItem> p_items;
            propertyItems.set(p_items);
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

        propertyItems.set(props);
    }
}

std::string EditorViewModel::generateDslFromAst(const std::shared_ptr<tooey::AstNode>& node, int indent) {
    if (!node) return "";
    std::stringstream ss;

    if (node->nodeType != "Root") {
        std::string indent_str(indent * 4, ' ');
        ss << indent_str << node->nodeType;
        if (!node->id.empty()) {
            ss << " id=" << node->id;
        }
        
        for (const auto& prop : node->properties) {
            if (prop.first == "width" || prop.first == "height" || prop.first == "spacing") {
                std::string val = prop.second.rawData;
                if (prop.second.type == tooey::PropertyType::String) {
                    val = "\"" + val + "\"";
                }
                ss << " " << prop.first << "=" << val;
            }
        }

        if (!node->children.empty()) {
            ss << ":\n";
            for (const auto& child : node->children) {
                ss << generateDslFromAst(child, indent + 1);
            }
        } else {
            ss << "\n";
            for (const auto& prop : node->properties) {
                if (prop.first != "width" && prop.first != "height" && prop.first != "spacing") {
                    std::string val = prop.second.rawData;
                    if (prop.second.type == tooey::PropertyType::String) {
                        val = "\"" + val + "\"";
                    } else if (prop.second.type == tooey::PropertyType::Localization) {
                        val = "@tr(\"" + val + "\")";
                    } else if (prop.second.type == tooey::PropertyType::Binding) {
                        val = "@binding." + val;
                    } else if (prop.second.type == tooey::PropertyType::Signal) {
                        val = "@signal." + val;
                    }
                    ss << indent_str << "    " << prop.first << ": " << val << "\n";
                }
            }
        }
    } else {
        for (const auto& child : node->children) {
            ss << generateDslFromAst(child, indent);
        }
    }

    return ss.str();
}

void EditorViewModel::deleteSelectedElement() {
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(hierarchyItems.get().size())) return;
    auto targetId = hierarchyItems.get()[selectedIndex].id;
    if (targetId == "mainLayout") return;

    record_history();

    struct Deleter {
        std::string targetId;
        bool delete_node(const std::shared_ptr<tooey::AstNode>& parent) {
            if (!parent) return false;
            for (auto it = parent->children.begin(); it != parent->children.end(); ++it) {
                if ((*it)->id == targetId) {
                    parent->children.erase(it);
                    return true;
                }
                if (delete_node(*it)) return true;
            }
            return false;
        }
    };

    Deleter del{targetId};
    if (del.delete_node(currentAst)) {
        selectedIndex = -1;
        is_updating_ = true;
        std::string serialized = generateDslFromAst(currentAst, 0);
        dslText.set(serialized);
        updateCanvasFromDsl(serialized);
        is_updating_ = false;
    }
}

void EditorViewModel::moveSelectedElementUp() {
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(hierarchyItems.get().size())) return;
    auto targetId = hierarchyItems.get()[selectedIndex].id;
    if (targetId == "mainLayout") return;

    record_history();

    struct Mover {
        std::string targetId;
        bool move_up(const std::shared_ptr<tooey::AstNode>& parent) {
            if (!parent) return false;
            for (auto it = parent->children.begin(); it != parent->children.end(); ++it) {
                if ((*it)->id == targetId) {
                    if (it != parent->children.begin()) {
                        std::iter_swap(it, it - 1);
                        return true;
                    }
                    return false;
                }
                if (move_up(*it)) return true;
            }
            return false;
        }
    };

    Mover mov{targetId};
    if (mov.move_up(currentAst)) {
        is_updating_ = true;
        std::string serialized = generateDslFromAst(currentAst, 0);
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

    record_history();

    struct Mover {
        std::string targetId;
        bool move_down(const std::shared_ptr<tooey::AstNode>& parent) {
            if (!parent) return false;
            for (auto it = parent->children.begin(); it != parent->children.end(); ++it) {
                if ((*it)->id == targetId) {
                    if (it + 1 != parent->children.end()) {
                        std::iter_swap(it, it + 1);
                        return true;
                    }
                    return false;
                }
                if (move_down(*it)) return true;
            }
            return false;
        }
    };

    Mover mov{targetId};
    if (mov.move_down(currentAst)) {
        is_updating_ = true;
        std::string serialized = generateDslFromAst(currentAst, 0);
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

void EditorViewModel::record_history() {
    std::string current = dslText.get();
    if (undo_stack_.empty() || undo_stack_.back() != current) {
        undo_stack_.push_back(current);
        redo_stack_.clear();
        if (undo_stack_.size() > 100) {
            undo_stack_.erase(undo_stack_.begin());
        }
    }
}

void EditorViewModel::undo() {
    if (undo_stack_.empty()) return;

    std::string previous_state = undo_stack_.back();
    undo_stack_.pop_back();

    std::string current_state = dslText.get();
    redo_stack_.push_back(current_state);

    is_updating_ = true;
    dslText.set(previous_state);
    updateCanvasFromDsl(previous_state);
    last_dsl_ = previous_state;
    is_updating_ = false;
}

void EditorViewModel::redo() {
    if (redo_stack_.empty()) return;

    std::string next_state = redo_stack_.back();
    redo_stack_.pop_back();

    std::string current_state = dslText.get();
    undo_stack_.push_back(current_state);

    is_updating_ = true;
    dslText.set(next_state);
    updateCanvasFromDsl(next_state);
    last_dsl_ = next_state;
    is_updating_ = false;
}

