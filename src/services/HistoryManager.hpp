#pragma once
#include <string>
#include <vector>
#include <cstddef>
#include <chrono>

namespace editor::services {

class HistoryManager {
public:
    explicit HistoryManager(std::size_t max_depth = 100);

    // Records a DSL snapshot. Respects a 1-second debounce to avoid recording
    // every single keystroke as a separate undo state, unless force is true.
    void record(const std::string& dsl, bool force = false);

    // Pops the previous state and pushes current_dsl onto redo stack.
    // Returns empty string if undo stack is empty.
    std::string undo(const std::string& current_dsl);

    // Pops the next state and pushes current_dsl onto undo stack.
    // Returns empty string if redo stack is empty.
    std::string redo(const std::string& current_dsl);

    bool can_undo() const noexcept;
    bool can_redo() const noexcept;

    void clear();

private:
    std::vector<std::string>                        undo_stack_;
    std::vector<std::string>                        redo_stack_;
    std::size_t                                     max_depth_;
    std::string                                     last_recorded_;
    std::chrono::steady_clock::time_point           last_record_time_;
};

} // namespace editor::services
