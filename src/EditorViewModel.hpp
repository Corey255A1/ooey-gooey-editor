#pragma once

#include "gooey/mvvmc/property.hpp"
#include "gooey/mvvmc/subscription_sink.hpp"
#include "tooey/ast.hpp"
#include <string>
#include <vector>
#include <memory>
#include <chrono>

#include "domain/models.hpp"

#include "services/HistoryManager.hpp"
#include "services/IAstSerializer.hpp"
#include "services/IAstMutator.hpp"
#include "services/IAstQuery.hpp"
#include "services/IDslCompilerService.hpp"

using editor::domain::ToolboxItem;
using editor::domain::HierarchyItem;
using editor::domain::PropertyItem;

class EditorViewModel : public gooey::mvvmc::ViewModel {
public:
    explicit EditorViewModel(
        std::shared_ptr<editor::services::IAstMutator> mutator,
        std::shared_ptr<editor::services::IAstSerializer> serializer,
        std::shared_ptr<editor::services::IAstQuery> query,
        std::shared_ptr<editor::services::IDslCompilerService> compiler,
        std::shared_ptr<editor::services::HistoryManager> history);

    // Observable properties bound directly by visual layout
    gooey::mvvmc::Property<std::vector<ToolboxItem>> toolboxItems;
    gooey::mvvmc::Property<std::vector<HierarchyItem>> hierarchyItems;
    gooey::mvvmc::Property<std::vector<PropertyItem>> propertyItems;
    gooey::mvvmc::Property<std::string> dslText;

    // Selected item index in the hierarchy
    int selectedIndex = -1;

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

    std::shared_ptr<editor::services::IAstMutator> mutator_;
    std::shared_ptr<editor::services::IAstSerializer> serializer_;
    std::shared_ptr<editor::services::IAstQuery> query_;
    std::shared_ptr<editor::services::IDslCompilerService> compiler_;
    std::shared_ptr<editor::services::HistoryManager> history_;

    bool is_updating_ = false;

    void rebuildHierarchyItems(const std::shared_ptr<tooey::AstNode>& node, int indent);
    void updatePropertyItems();
};

