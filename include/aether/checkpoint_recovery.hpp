#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <iostream>

namespace aether {

struct StateSnapshot {
    uint64_t snapshot_id;
    std::string task_checkpoint_name;
    uint64_t timestamp_ms;
    std::unordered_map<std::string, std::string> state_variables;
};

class CheckpointManager {
public:
    CheckpointManager() : next_id_(1) {}

    // Save transactional state snapshot (Push to rollback stack)
    // Time Complexity: O(K) where K is number of active state variables
    uint64_t create_checkpoint(const std::string& checkpoint_name,
                               const std::unordered_map<std::string, std::string>& state) {
        uint64_t now_ms = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );

        StateSnapshot snap;
        snap.snapshot_id = next_id_++;
        snap.task_checkpoint_name = checkpoint_name;
        snap.timestamp_ms = now_ms;
        snap.state_variables = state;

        snapshot_stack_.push_back(snap);
        return snap.snapshot_id;
    }

    // Rollback to previous verified checkpoint
    // Time Complexity: O(1) stack pop
    bool rollback_to_last_checkpoint(std::unordered_map<std::string, std::string>& restored_state,
                                     std::string& rolled_back_name) {
        if (snapshot_stack_.empty()) {
            return false;
        }

        StateSnapshot last = snapshot_stack_.back();
        snapshot_stack_.pop_back();

        restored_state = last.state_variables;
        rolled_back_name = last.task_checkpoint_name;
        return true;
    }

    size_t checkpoint_depth() const {
        return snapshot_stack_.size();
    }

    void clear() {
        snapshot_stack_.clear();
    }

private:
    uint64_t next_id_;
    std::vector<StateSnapshot> snapshot_stack_;
};

} // namespace aether
