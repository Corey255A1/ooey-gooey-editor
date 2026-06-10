#include "HistoryManager.hpp"

namespace editor::services {

HistoryManager::HistoryManager(std::size_t max_depth)
    : max_depth_(max_depth)
    , last_record_time_(std::chrono::steady_clock::now())
{}

void HistoryManager::record(const std::string& dsl, bool force) {
    if (dsl.empty()) return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_record_time_).count();

    if (force) {
        // Force immediate record of the passed state
        if (undo_stack_.empty() || undo_stack_.back() != dsl) {
            undo_stack_.push_back(dsl);
            redo_stack_.clear();
            if (undo_stack_.size() > max_depth_) {
                undo_stack_.erase(undo_stack_.begin());
            }
        }
        last_recorded_ = dsl;
        last_record_time_ = now;
    } else {
        // Debounced typing record
        if (elapsed > 1000 || last_recorded_.empty()) {
            if (!last_recorded_.empty() && (undo_stack_.empty() || undo_stack_.back() != last_recorded_)) {
                undo_stack_.push_back(last_recorded_);
                redo_stack_.clear();
                if (undo_stack_.size() > max_depth_) {
                    undo_stack_.erase(undo_stack_.begin());
                }
            }
            last_record_time_ = now;
        }
        last_recorded_ = dsl;
    }
}

std::string HistoryManager::undo(const std::string& current_dsl) {
    if (undo_stack_.empty()) return "";

    std::string previous_state = undo_stack_.back();
    undo_stack_.pop_back();

    redo_stack_.push_back(current_dsl);
    last_recorded_ = previous_state;
    last_record_time_ = std::chrono::steady_clock::now();

    return previous_state;
}

std::string HistoryManager::redo(const std::string& current_dsl) {
    if (redo_stack_.empty()) return "";

    std::string next_state = redo_stack_.back();
    redo_stack_.pop_back();

    undo_stack_.push_back(current_dsl);
    last_recorded_ = next_state;
    last_record_time_ = std::chrono::steady_clock::now();

    return next_state;
}

bool HistoryManager::can_undo() const noexcept {
    return !undo_stack_.empty();
}

bool HistoryManager::can_redo() const noexcept {
    return !redo_stack_.empty();
}

void HistoryManager::clear() {
    undo_stack_.clear();
    redo_stack_.clear();
    last_recorded_.clear();
}

} // namespace editor::services
