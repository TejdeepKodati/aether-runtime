#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <unordered_map>

#include "aether/task_dag.hpp"
#include "aether/priority_queue.hpp"
#include "aether/permission_guard.hpp"
#include "aether/hierarchical_memory.hpp"
#include "aether/invariant_verifier.hpp"
#include "aether/checkpoint_recovery.hpp"
#include "aether/runtime_engine.hpp"

using namespace aether;

void print_banner() {
    std::cout << R"(
========================================================================
    ___         __  __                ____              __  _           
   /   | ____  / /_/ /_  ___  _____  / __ \__  ______  / /_(_)___ ___  ___ 
  / /| |/ _ \/ __/ __ \/ _ \/ ___/ / /_/ / / / / __ \/ __/ / __ `__ \/ _ \
 / ___ /  __/ /_/ / / /  __/ /    / _, _/ /_/ / / / / /_/ / / / / / /  __/
/_/  |_\___/\__/_/ /_/\___/_/    /_/ |_|\__,_/_/ /_/\__/_/_/ /_/ /_/\___/ 
                                                                           
   Deterministic Autonomous Agent Runtime & Invariant Verification Engine
   Engineered in Modern C++17/20 | Author: Tejdeep Kodati
========================================================================
)" << std::endl;
}

void run_enterprise_workflow_demo() {
    std::cout << "\n[Workflow] Initiating Incident Remediation Workflow (DAG Execution)..." << std::endl;

    AgentRuntimeEngine engine;

    // 1. Setup Permissions
    engine.permissions().register_tool_policy("fetch_metrics", ActionRisk::ReadOnly, "monitoring");
    engine.permissions().register_tool_policy("triage_error", ActionRisk::Safe, "diagnostics");
    engine.permissions().register_tool_policy("query_runbook", ActionRisk::ReadOnly, "knowledge_base");
    engine.permissions().register_tool_policy("restart_service", ActionRisk::Sensitive, "infrastructure");
    engine.permissions().register_tool_policy("purge_database_records", ActionRisk::Destructive, "database");

    // 2. Pre-seed Hierarchical Memory
    std::cout << "[Memory] Initializing Multi-Tiered Enterprise Memory..." << std::endl;
    // L2 Trie: Company entity routes and policies
    engine.memory().l2().insert("runbook:devops:service_oom", "Runbook #402: Check memory leak in worker pool and restart container");
    engine.memory().l2().insert("policy:devops:restart_window", "Service restarts permitted automatically if error rate > 5%");

    // L3 Vector: Dense semantic embeddings for incident descriptions
    engine.memory().l3().add_document("inc_901", "High latency on payment gateway due to pool exhaustion", {0.8f, 0.2f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f});
    engine.memory().l3().add_document("inc_902", "OOM killer invoked on auth service pod 3", {0.1f, 0.9f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f});

    // 3. Construct DAG Workflow
    TaskDAG dag;

    TaskNode n1;
    n1.id = "step1_metrics";
    n1.name = "Fetch Pod Prometheus Metrics";
    n1.action_type = "fetch_metrics";
    n1.execution_fn = [](const std::unordered_map<std::string, std::string>&) {
        return "{\"pod\": \"auth-service-pod-3\", \"status\": \"OOMKilled\", \"memory_usage_mb\": 2048}";
    };

    TaskNode n2;
    n2.id = "step2_semantic_search";
    n2.name = "Retrieve Similar Historical Incidents";
    n2.action_type = "query_runbook";
    n2.execution_fn = [&](const std::unordered_map<std::string, std::string>&) -> std::string {
        auto search = engine.memory().l3().search({0.1f, 0.85f, 0.15f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, 1);
        if (!search.empty()) {
            return "{\"matched_incident\": \"" + search[0].id + "\", \"similarity\": " + std::to_string(search[0].score) + "}";
        }
        return "{\"matched_incident\": \"none\"}";
    };

    TaskNode n3;
    n3.id = "step3_runbook_lookup";
    n3.name = "Resolve Remediation Runbook from Trie";
    n3.action_type = "query_runbook";
    n3.execution_fn = [&](const std::unordered_map<std::string, std::string>&) {
        std::string runbook_text;
        engine.memory().l2().find("runbook:devops:service_oom", runbook_text);
        return "{\"action\": \"restart\", \"runbook_guidance\": \"" + runbook_text + "\"}";
    };

    TaskNode n4;
    n4.id = "step4_restart";
    n4.name = "Execute Graceful Service Restart";
    n4.action_type = "restart_service";
    n4.execution_fn = [](const std::unordered_map<std::string, std::string>&) {
        return "{\"restart_status\": \"success\", \"pod\": \"auth-service-pod-3\", \"healthy\": true}";
    };

    dag.add_node(n1);
    dag.add_node(n2);
    dag.add_node(n3);
    dag.add_node(n4);

    dag.add_dependency("step1_metrics", "step2_semantic_search");
    dag.add_dependency("step1_metrics", "step3_runbook_lookup");
    dag.add_dependency("step2_semantic_search", "step4_restart");
    dag.add_dependency("step3_runbook_lookup", "step4_restart");

    std::unordered_map<std::string, std::string> initial_state = {
        {"cluster", "prod-us-east-1"},
        {"alert_id", "ALRT-8841"}
    };

    std::cout << "[DAG] Executing Kahn's topological plan with Operator role..." << std::endl;
    auto result = engine.execute_dag(dag, AgentRole::Operator, initial_state, false);

    std::cout << "\n================ Workflow Execution Trace ================" << std::endl;
    std::cout << "Workflow ID : " << result.workflow_id << std::endl;
    std::cout << "Execution Status: " << (result.success ? "SUCCESS" : "FAILED") << std::endl;
    std::cout << "Completed Tasks : " << result.completed_tasks << " / " << result.total_tasks << std::endl;
    std::cout << "Total Latency   : " << result.total_execution_time_us << " microseconds" << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;

    for (const auto& entry : result.trace) {
        std::cout << "Task [" << entry.task_id << "] - " << entry.task_name << "\n";
        std::cout << "  Status      : " << entry.status << "\n";
        std::cout << "  Verification: " << entry.verification_status << "\n";
        std::cout << "  Latency     : " << entry.latency_us << " us\n";
        std::cout << "  Output      : " << entry.output << "\n";
        std::cout << std::endl;
    }

    std::cout << "==========================================================" << std::endl;
}

int main() {
    print_banner();
    run_enterprise_workflow_demo();
    return 0;
}
