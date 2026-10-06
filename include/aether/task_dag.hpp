#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <functional>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <iostream>

namespace aether {

enum class NodeStatus {
    Pending,
    Ready,
    Running,
    Completed,
    Failed,
    Verified,
    RolledBack
};

inline std::string status_to_string(NodeStatus s) {
    switch (s) {
        case NodeStatus::Pending: return "Pending";
        case NodeStatus::Ready: return "Ready";
        case NodeStatus::Running: return "Running";
        case NodeStatus::Completed: return "Completed";
        case NodeStatus::Failed: return "Failed";
        case NodeStatus::Verified: return "Verified";
        case NodeStatus::RolledBack: return "RolledBack";
    }
    return "Unknown";
}

struct TaskNode {
    std::string id;
    std::string name;
    std::string action_type; // e.g. "api_call", "file_mutation", "db_query", "llm_plan"
    std::string payload;
    std::vector<std::string> dependencies; // Prerequisite node IDs
    NodeStatus status{NodeStatus::Pending};
    std::string result;
    std::string error_message;
    int retry_count{0};
    int max_retries{3};

    std::function<std::string(const std::unordered_map<std::string, std::string>& context)> execution_fn;
};

class TaskDAG {
public:
    TaskDAG() = default;

    void add_node(const TaskNode& node) {
        if (nodes_.find(node.id) != nodes_.end()) {
            throw std::invalid_argument("Task node already exists: " + node.id);
        }
        nodes_[node.id] = node;
        adjacency_list_[node.id] = {};
        in_degree_[node.id] = 0;
    }

    void add_dependency(const std::string& from, const std::string& to) {
        if (nodes_.find(from) == nodes_.end() || nodes_.find(to) == nodes_.end()) {
            throw std::invalid_argument("Nodes must exist in DAG before adding dependency: " + from + " -> " + to);
        }
        adjacency_list_[from].push_back(to);
        nodes_[to].dependencies.push_back(from);
        in_degree_[to]++;
    }

    // Algorithmic validation: Cycle detection and Kahn's algorithm
    // Returns topologically ordered execution layers (batches of independent tasks that can run in parallel)
    // Time Complexity: O(V + E), Space Complexity: O(V + E)
    std::vector<std::vector<std::string>> compute_execution_plan() const {
        std::unordered_map<std::string, int> current_in_degree = in_degree_;
        std::queue<std::string> zero_in_degree_queue;

        for (const auto& [node_id, deg] : current_in_degree) {
            if (deg == 0) {
                zero_in_degree_queue.push(node_id);
            }
        }

        std::vector<std::vector<std::string>> execution_layers;
        size_t processed_nodes = 0;

        while (!zero_in_degree_queue.empty()) {
            size_t layer_size = zero_in_degree_queue.size();
            std::vector<std::string> current_layer;

            for (size_t i = 0; i < layer_size; ++i) {
                std::string u = zero_in_degree_queue.front();
                zero_in_degree_queue.pop();
                current_layer.push_back(u);
                processed_nodes++;

                auto it = adjacency_list_.find(u);
                if (it != adjacency_list_.end()) {
                    for (const auto& v : it->second) {
                        current_in_degree[v]--;
                        if (current_in_degree[v] == 0) {
                            zero_in_degree_queue.push(v);
                        }
                    }
                }
            }
            execution_layers.push_back(current_layer);
        }

        if (processed_nodes != nodes_.size()) {
            throw std::runtime_error("DAG cycle detected! Dependency loop makes execution impossible.");
        }

        return execution_layers;
    }

    TaskNode* get_node(const std::string& id) {
        auto it = nodes_.find(id);
        if (it != nodes_.end()) {
            return &it->second;
        }
        return nullptr;
    }

    const TaskNode* get_node(const std::string& id) const {
        auto it = nodes_.find(id);
        if (it != nodes_.end()) {
            return &it->second;
        }
        return nullptr;
    }

    const std::unordered_map<std::string, TaskNode>& get_all_nodes() const {
        return nodes_;
    }

    size_t size() const {
        return nodes_.size();
    }

    // Dynamic re-planning: Replace failed node with an alternate sub-pipeline
    void replace_node_with_subgraph(const std::string& failed_node_id, const std::vector<TaskNode>& fallback_nodes) {
        if (nodes_.find(failed_node_id) == nodes_.end()) {
            throw std::invalid_argument("Cannot replace non-existent node: " + failed_node_id);
        }

        std::vector<std::string> original_deps = nodes_[failed_node_id].dependencies;
        std::vector<std::string> original_successors = adjacency_list_[failed_node_id];

        // Mark failed node as rolled back
        nodes_[failed_node_id].status = NodeStatus::RolledBack;

        if (fallback_nodes.empty()) return;

        // Add fallback nodes
        for (const auto& fn : fallback_nodes) {
            add_node(fn);
        }

        // Connect original prerequisites to the first fallback node
        std::string first_fallback = fallback_nodes.front().id;
        for (const auto& dep : original_deps) {
            add_dependency(dep, first_fallback);
        }

        // Connect chain of fallback nodes
        for (size_t i = 0; i + 1 < fallback_nodes.size(); ++i) {
            add_dependency(fallback_nodes[i].id, fallback_nodes[i+1].id);
        }

        // Connect last fallback node to original successors
        std::string last_fallback = fallback_nodes.back().id;
        for (const auto& succ : original_successors) {
            add_dependency(last_fallback, succ);
        }
    }

private:
    std::unordered_map<std::string, TaskNode> nodes_;
    std::unordered_map<std::string, std::vector<std::string>> adjacency_list_;
    std::unordered_map<std::string, int> in_degree_;
};

} // namespace aether
