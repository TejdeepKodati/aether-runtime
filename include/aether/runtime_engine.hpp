#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>

#include "aether/task_dag.hpp"
#include "aether/priority_queue.hpp"
#include "aether/permission_guard.hpp"
#include "aether/hierarchical_memory.hpp"
#include "aether/invariant_verifier.hpp"
#include "aether/checkpoint_recovery.hpp"

namespace aether {

struct ExecutionTraceEntry {
    std::string task_id;
    std::string task_name;
    std::string status;
    uint64_t latency_us;
    std::string output;
    std::string verification_status;
    bool rolled_back{false};
};

struct WorkflowResult {
    bool success{false};
    std::string workflow_id;
    size_t total_tasks{0};
    size_t completed_tasks{0};
    size_t rolled_back_tasks{0};
    uint64_t total_execution_time_us{0};
    std::vector<ExecutionTraceEntry> trace;
    std::unordered_map<std::string, std::string> final_state;
};

class AgentRuntimeEngine {
public:
    AgentRuntimeEngine(double rate_limit_capacity = 100.0, double refill_rate = 50.0)
        : permission_guard_(rate_limit_capacity, refill_rate),
          memory_manager_(128, 8) {}

    PermissionGuard& permissions() { return permission_guard_; }
    HierarchicalMemoryManager& memory() { return memory_manager_; }
    InvariantEngine& verifier() { return invariant_engine_; }
    CheckpointManager& checkpoints() { return checkpoint_manager_; }
    PriorityTaskQueue& queue() { return task_queue_; }

    WorkflowResult execute_dag(
        TaskDAG& dag,
        AgentRole role,
        std::unordered_map<std::string, std::string> initial_state,
        bool hitl_approval_granted = false
    ) {
        WorkflowResult result;
        result.workflow_id = "wf-" + std::to_string(PriorityTaskQueue::current_time_ms());
        result.total_tasks = dag.size();
        result.final_state = initial_state;

        auto start_total = std::chrono::high_resolution_clock::now();

        // 1. Topological planning phase
        std::vector<std::vector<std::string>> layers;
        try {
            layers = dag.compute_execution_plan();
        } catch (const std::exception& e) {
            result.success = false;
            std::cerr << "[Runtime] DAG compilation error: " << e.what() << std::endl;
            return result;
        }

        // 2. Multi-tier Layer-by-Layer Execution Loop
        for (size_t layer_idx = 0; layer_idx < layers.size(); ++layer_idx) {
            const auto& current_layer = layers[layer_idx];

            for (const auto& node_id : current_layer) {
                TaskNode* node = dag.get_node(node_id);
                if (!node) continue;

                auto node_start = std::chrono::high_resolution_clock::now();
                ExecutionTraceEntry trace_entry;
                trace_entry.task_id = node->id;
                trace_entry.task_name = node->name;

                // Step A: Checkpoint state before execution for deterministic rollback capability
                checkpoint_manager_.create_checkpoint(node->id, result.final_state);

                // Step B: Security & Permission Guard evaluation
                auto perm_check = permission_guard_.evaluate_action(
                    role, node->action_type, node->name, hitl_approval_granted
                );

                if (!perm_check.allowed) {
                    node->status = NodeStatus::Failed;
                    node->error_message = perm_check.reason;
                    trace_entry.status = "Blocked: " + perm_check.reason;
                    trace_entry.verification_status = "Skipped";

                    // Trigger state rollback
                    std::string rolled_name;
                    checkpoint_manager_.rollback_to_last_checkpoint(result.final_state, rolled_name);
                    trace_entry.rolled_back = true;
                    result.rolled_back_tasks++;

                    auto node_end = std::chrono::high_resolution_clock::now();
                    trace_entry.latency_us = std::chrono::duration_cast<std::chrono::microseconds>(node_end - node_start).count();
                    result.trace.push_back(trace_entry);

                    result.success = false;
                    return result; // Halt execution if permission fails
                }

                // Step C: Execution
                node->status = NodeStatus::Running;
                std::string output;
                try {
                    if (node->execution_fn) {
                        output = node->execution_fn(result.final_state);
                    } else {
                        output = node->payload;
                    }
                    node->result = output;
                } catch (const std::exception& e) {
                    node->status = NodeStatus::Failed;
                    node->error_message = e.what();
                    trace_entry.status = "Execution Exception: " + std::string(e.what());

                    // Rollback
                    std::string rolled_name;
                    checkpoint_manager_.rollback_to_last_checkpoint(result.final_state, rolled_name);
                    trace_entry.rolled_back = true;
                    result.rolled_back_tasks++;

                    auto node_end = std::chrono::high_resolution_clock::now();
                    trace_entry.latency_us = std::chrono::duration_cast<std::chrono::microseconds>(node_end - node_start).count();
                    result.trace.push_back(trace_entry);

                    result.success = false;
                    return result;
                }

                // Step D: Invariant Verification
                auto verify_report = invariant_engine_.verify_output(node->id, output);
                if (!verify_report.passed) {
                    node->status = NodeStatus::Failed;
                    node->error_message = verify_report.details;
                    trace_entry.status = "Verification Failed";
                    trace_entry.verification_status = verify_report.details;

                    // Deterministic Rollback
                    std::string rolled_name;
                    checkpoint_manager_.rollback_to_last_checkpoint(result.final_state, rolled_name);
                    trace_entry.rolled_back = true;
                    result.rolled_back_tasks++;

                    auto node_end = std::chrono::high_resolution_clock::now();
                    trace_entry.latency_us = std::chrono::duration_cast<std::chrono::microseconds>(node_end - node_start).count();
                    result.trace.push_back(trace_entry);

                    result.success = false;
                    return result;
                }

                // Step E: Verified! Commit to state and L1 Working Memory
                node->status = NodeStatus::Verified;
                result.final_state["output:" + node->id] = output;
                memory_manager_.l1().put("task_result:" + node->id, output);

                result.completed_tasks++;
                trace_entry.status = "Verified";
                trace_entry.output = output;
                trace_entry.verification_status = "Passed (" + std::to_string(verify_report.passed_invariants.size()) + " invariants)";

                auto node_end = std::chrono::high_resolution_clock::now();
                trace_entry.latency_us = std::chrono::duration_cast<std::chrono::microseconds>(node_end - node_start).count();
                result.trace.push_back(trace_entry);
            }
        }

        auto end_total = std::chrono::high_resolution_clock::now();
        result.total_execution_time_us = std::chrono::duration_cast<std::chrono::microseconds>(end_total - start_total).count();
        result.success = (result.completed_tasks == result.total_tasks);

        return result;
    }

private:
    PermissionGuard permission_guard_;
    HierarchicalMemoryManager memory_manager_;
    InvariantEngine invariant_engine_;
    CheckpointManager checkpoint_manager_;
    PriorityTaskQueue task_queue_;
};

} // namespace aether
