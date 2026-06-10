#include "services/HistoryManager.hpp"
#include <cassert>
#include <iostream>
#include <thread>

void test_basic_undo_redo() {
    editor::services::HistoryManager history(10);
    assert(!history.can_undo());
    assert(!history.can_redo());

    // Record initial state (State A is the state before transitioning to State B)
    history.record("State A", true);
    assert(history.can_undo()); // We can undo back to State A

    // Record State B (before transitioning to State C)
    history.record("State B", true);
    assert(history.can_undo());

    // Undo from State B/C back to State B
    std::string prev = history.undo("State C");
    assert(prev == "State B");
    assert(history.can_undo());
    assert(history.can_redo());

    // Redo back to State C
    std::string next = history.redo(prev);
    assert(next == "State C");
    assert(history.can_undo());
    assert(!history.can_redo());
}

void test_empty_stacks() {
    editor::services::HistoryManager history(10);
    assert(history.undo("State A").empty());
    assert(history.redo("State A").empty());
}

void test_deduplication() {
    editor::services::HistoryManager history(10);
    history.record("State A", true);
    history.record("State A", true); // Duplicate should be ignored
    
    assert(history.can_undo());
    assert(history.undo("State B") == "State A");
    assert(!history.can_undo());
}

void test_max_depth() {
    editor::services::HistoryManager history(3);
    history.record("State 1", true);
    history.record("State 2", true);
    history.record("State 3", true);
    history.record("State 4", true);

    // With depth 3, the states kept in undo stack should be 1, 2, 3 (Wait, when we recorded 4, 1, 2, 3 were kept)
    // Actually, record("State X", true) does:
    // if undo_stack_.empty() || undo_stack_.back() != dsl:
    //   undo_stack_.push_back(dsl)
    // So for State 1, 2, 3, 4:
    // 1 -> undo_stack: [1]
    // 2 -> undo_stack: [1, 2]
    // 3 -> undo_stack: [1, 2, 3]
    // We expect the undo stack to contain ["State 2", "State 3", "State 4"] since max depth is 3.
    // Wait, in EditorViewModel, when we perform an action, we call `record(current_state_before_action, true)`.
    // So the state recorded is the state BEFORE modification.
    // If we have State 1, 2, 3, 4, the undo stack has 1, 2, 3. The current state is 4.
    // So if depth is 3:
    // history.record("1", true) -> undo_stack: [1]
    // history.record("2", true) -> undo_stack: [1, 2]
    // history.record("3", true) -> undo_stack: [1, 2, 3]
    // history.record("4", true) -> undo_stack: [2, 3, 4] (evicted 1)
    
    assert(history.undo("State 5") == "State 4");
    assert(history.undo("State 4") == "State 3");
    assert(history.undo("State 3") == "State 2");
    assert(!history.can_undo());
}

void test_debounce() {
    editor::services::HistoryManager history(10);
    history.record("State A", false);
    
    // Quick typing (less than 1s)
    history.record("State A1", false);
    history.record("State A2", false);

    // Debounced, so undo_stack should still be empty
    assert(!history.can_undo());

    // Sleep for 1.1 seconds to trigger debounce interval
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    history.record("State B", false); // This should commit State A2 to undo stack
    assert(history.can_undo());
    assert(history.undo("State B") == "State A2");
}

void test_clear() {
    editor::services::HistoryManager history(10);
    history.record("State A", true);
    history.record("State B", true);
    history.clear();
    assert(!history.can_undo());
    assert(!history.can_redo());
}

int main() {
    test_basic_undo_redo();
    test_empty_stacks();
    test_deduplication();
    test_max_depth();
    test_debounce();
    test_clear();
    std::cout << "All HistoryManager tests passed successfully!\n";
    return 0;
}
