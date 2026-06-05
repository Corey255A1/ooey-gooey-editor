#pragma once

#include "gooey/mvvmc/property.hpp"
#include "gooey/mvvmc/subscription_sink.hpp"
#include "tooey/ast.hpp"
#include <string>
#include <vector>
#include <memory>
#include <chrono>

struct ToolboxItem {
    std::string name;
};

struct HierarchyItem {
    std::string name;
    int indent;
    std::string id;
};

struct PropertyItem {
    std::string name;
    std::string value;
};

class EditorViewModel : public gooey::mvvmc::ViewModel {
public:
    EditorViewModel();

    // Observable properties bound directly by visual layout
    gooey::mvvmc::Property<std::vector<ToolboxItem>> toolboxItems;
    gooey::mvvmc::Property<std::vector<HierarchyItem>> hierarchyItems;
    gooey::mvvmc::Property<std::vector<PropertyItem>> propertyItems;
    gooey::mvvmc::Property<std::string> dslText;

    // Selected item index in the hierarchy
    int selectedIndex = -1;
    bool is_updating_ = false;

    // View actions
    void selectElement(int index);
    void updateProperty(int index, const std::string& newValue);
    void addControlToCanvas(const std::string& type);
    void updateCanvasFromDsl(const std::string& dsl);
    void deleteSelectedElement();
    void moveSelectedElementUp();
    void moveSelectedElementDown();
    void undo();
    void redo();

    // Active parsed AST representation
    std::shared_ptr<tooey::AstNode> currentAst;

private:
    gooey::mvvmc::SubscriptionSink sink_;

    std::vector<std::string> undo_stack_;
    std::vector<std::string> redo_stack_;
    void record_history();
    std::string last_dsl_;
    std::chrono::steady_clock::time_point last_typing_record_time_;

    void rebuildHierarchyItems(const std::shared_ptr<tooey::AstNode>& node, int indent);
    void updatePropertyItems();
    std::string generateDslFromAst(const std::shared_ptr<tooey::AstNode>& node, int indent);
};

