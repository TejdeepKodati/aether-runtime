#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <cmath>

#include "aether/task_dag.hpp"
#include "aether/priority_queue.hpp"
#include "aether/permission_guard.hpp"
#include "aether/hierarchical_memory.hpp"
#include "aether/invariant_verifier.hpp"
#include "aether/checkpoint_recovery.hpp"
#include "aether/runtime_engine.hpp"

using namespace aether;

void run_dag_tests() {
    std::cout << "[TEST] 1. TaskDAG Topological Sort & Cycle Detection..." << std::endl;
    TaskDAG dag;

    dag.add_node({"t1", "Fetch Customer Record", "db_query", "cust_101"});
    dag.add_node({"t2", "Calculate Billing Adjustment", "calc", "amt_50"});
    dag.add_node({"t3", "Audit Compliance Policy", "policy_eval", "standard"});
    dag.add_node({"t4", "Apply Ledger Mutation", "db_mutate", "commit"});

    dag.add_dependency("t1", "t2");
    dag.add_dependency("t1", "t3");
    dag.add_dependency("t2", "t4");
    dag.add_dependency("t3", "t4");

    auto plan = dag.compute_execution_plan();
    assert(plan.size() == 3);
    assert(plan[0].size() == 1 && plan[0][0] == "t1");
    assert(plan[1].size() == 2); // t2 and t3 can execute concurrently
    assert(plan[2].size() == 1 && plan[2][0] == "t4");

    // Test Cycle Detection
    TaskDAG cyclic_dag;
    cyclic_dag.add_node({"a", "Node A", "op", ""});
    cyclic_dag.add_node({"b", "Node B", "op", ""});
    cyclic_dag.add_dependency("a", "b");
    cyclic_dag.add_dependency("b", "a");

    bool caught_cycle = false;
    try {
        cyclic_dag.compute_execution_plan();
    } catch (const std::runtime_error&) {
        caught_cycle = true;
    }
    assert(caught_cycle && "DAG must reject cyclic dependencies!");

    std::cout << "       PASS: Topological layering and cycle detection verified." << std::endl;
}

void run_queue_tests() {
    std::cout << "[TEST] 2. PriorityTaskQueue & Dead Letter Queue..." << std::endl;
    PriorityTaskQueue queue;

    queue.enqueue({"t_low", "Background Cleanup", PriorityLevel::Low, "clean_cache"});
    queue.enqueue({"t_crit", "Security Emergency Lock", PriorityLevel::Critical, "lock_tenant"});
    queue.enqueue({"t_norm", "Process Order", PriorityLevel::Normal, "order_99"});

    QueueTask first;
    assert(queue.dequeue(first));
    assert(first.id == "t_crit"); // Critical priority popped first

    QueueTask second;
    assert(queue.dequeue(second));
    assert(second.id == "t_norm"); // Normal priority popped next

    // Test DLQ routing on failures
    PriorityTaskQueue retry_q;
    QueueTask failing_task;
    failing_task.id = "failing_job";
    failing_task.task_name = "Network Sync";
    failing_task.max_retries = 2;
    failing_task.base_backoff_ms = 1;
    failing_task.run_fn = [](const std::string&) -> bool {
        throw std::runtime_error("Downstream API timeout");
    };

    retry_q.enqueue(failing_task);
    // Attempt 1 -> Fails, retried with backoff
    retry_q.process_next_task();
    sleep_ms(5);
    // Attempt 2 -> Fails, retried with backoff
    retry_q.process_next_task();
    sleep_ms(10);
    // Attempt 3 -> Fails, max retries reached, routes to DLQ
    retry_q.process_next_task();

    assert(retry_q.dlq_size() == 1);
    auto dlq_items = retry_q.get_dlq_items();
    assert(dlq_items[0].id == "failing_job");
    assert(dlq_items[0].failed_attempts == 3);

    std::cout << "       PASS: Priority ordering, exponential retry, and DLQ verified." << std::endl;
}

void run_memory_tests() {
    std::cout << "[TEST] 3. Hierarchical Memory (L1 LRU, L2 Trie, L3 Vector Search)..." << std::endl;

    // L1 LRU Cache
    LRUCache lru(2);
    lru.put("key1", "val1");
    lru.put("key2", "val2");

    std::string val;
    assert(lru.get("key1", val) && val == "val1"); // Access key1, key2 is now least recently used

    lru.put("key3", "val3"); // Evicts key2
    assert(!lru.get("key2", val)); // key2 was evicted
    assert(lru.get("key1", val) && val == "val1");
    assert(lru.get("key3", val) && val == "val3");

    // L2 Prefix Trie
    PrefixTrie trie;
    trie.insert("policy:hr:leave:annual", "20 days paid leave per fiscal year");
    trie.insert("policy:hr:leave:sick", "10 days medical leave with doctor certificate");
    trie.insert("policy:finance:refund:limit", "$500 manager threshold");

    std::string trie_val;
    assert(trie.find("policy:hr:leave:annual", trie_val));
    assert(trie_val == "20 days paid leave per fiscal year");

    auto prefix_results = trie.find_by_prefix("policy:hr:leave");
    assert(prefix_results.size() == 2);

    // L3 Dense Vector Memory
    VectorMemoryIndex vec_idx(4);
    vec_idx.add_document("doc_hr", "Employee benefits overview", {1.0f, 0.0f, 0.0f, 0.0f});
    vec_idx.add_document("doc_devops", "Kubernetes cluster runbook", {0.0f, 1.0f, 0.0f, 0.0f});
    vec_idx.add_document("doc_finance", "Quarterly budget reconciliation", {0.0f, 0.0f, 1.0f, 0.0f});

    auto search_results = vec_idx.search({0.95f, 0.05f, 0.0f, 0.0f}, 1);
    assert(!search_results.empty());
    assert(search_results[0].id == "doc_hr");
    assert(search_results[0].score > 0.9f);

    std::cout << "       PASS: LRU O(1) eviction, Trie prefix indexing, and Vector k-NN verified." << std::endl;
}

void run_permission_and_invariant_tests() {
    std::cout << "[TEST] 4. PermissionGuard & InvariantEngine Verification..." << std::endl;

    PermissionGuard guard;
    guard.register_tool_policy("file_read", ActionRisk::ReadOnly, "workspace");
    guard.register_tool_policy("database_drop", ActionRisk::Destructive, "production");

    // Viewer cannot execute destructive actions
    auto res1 = guard.evaluate_action(AgentRole::Viewer, "database_drop", "users_table", false);
    assert(!res1.allowed);
    assert(res1.requires_hitl);

    // Approved human allows destructive action for Admin
    auto res2 = guard.evaluate_action(AgentRole::Admin, "database_drop", "temp_table", true);
    assert(res2.allowed);

    // Invariant Engine checks
    InvariantEngine verifier;
    auto rep_pass = verifier.verify_output("task_1", "{\"status\": \"ok\", \"records_updated\": 14}");
    assert(rep_pass.passed);

    // Invariant violation: Leaked API secret
    auto rep_fail_secret = verifier.verify_output("task_2", "Leaked: AIzaSyD98234jhsdf");
    assert(!rep_fail_secret.passed);

    // Invariant violation: Unbalanced JSON braces
    auto rep_fail_json = verifier.verify_output("task_3", "{\"status\": \"error\", ");
    assert(!rep_fail_json.passed);

    std::cout << "       PASS: Permission gating, HITL enforcement, and invariant checks verified." << std::endl;
}

void run_runtime_engine_integration_test() {
    std::cout << "[TEST] 5. End-to-End AgentRuntimeEngine Workflow Execution..." << std::endl;

    AgentRuntimeEngine engine;
    engine.permissions().register_tool_policy("read_order", ActionRisk::ReadOnly, "ecommerce");
    engine.permissions().register_tool_policy("calculate_tax", ActionRisk::Safe, "finance");
    engine.permissions().register_tool_policy("generate_receipt", ActionRisk::Sensitive, "invoicing");

    TaskDAG dag;
    TaskNode n1;
    n1.id = "step1";
    n1.name = "Read Order Details";
    n1.action_type = "read_order";
    n1.execution_fn = [](const std::unordered_map<std::string, std::string>&) {
        return "{\"order_id\": \"ord_5521\", \"subtotal\": 120.00}";
    };

    TaskNode n2;
    n2.id = "step2";
    n2.name = "Compute Sales Tax";
    n2.action_type = "calculate_tax";
    n2.execution_fn = [](const std::unordered_map<std::string, std::string>& ctx) {
        return "{\"tax_amount\": 12.00, \"total\": 132.00}";
    };

    TaskNode n3;
    n3.id = "step3";
    n3.name = "Generate Customer Invoice";
    n3.action_type = "generate_receipt";
    n3.execution_fn = [](const std::unordered_map<std::string, std::string>& ctx) {
        return "{\"invoice_id\": \"inv_9981\", \"status\": \"generated\"}";
    };

    dag.add_node(n1);
    dag.add_node(n2);
    dag.add_node(n3);

    dag.add_dependency("step1", "step2");
    dag.add_dependency("step2", "step3");

    std::unordered_map<std::string, std::string> init_state = {{"user_id", "u_88"}};
    auto result = engine.execute_dag(dag, AgentRole::Operator, init_state, false);

    assert(result.success);
    assert(result.completed_tasks == 3);
    assert(result.rolled_back_tasks == 0);
    assert(result.trace.size() == 3);

    // Verify state was saved to working memory
    std::string l1_cached;
    assert(engine.memory().l1().get("task_result:step3", l1_cached));

    std::cout << "       PASS: Complete Agent DAG workflow executed and verified." << std::endl;
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "   AetherRuntime Comprehensive Test Suite                " << std::endl;
    std::cout << "   Autonomous Agent Runtime & Invariant Verification     " << std::endl;
    std::cout << "==========================================================" << std::endl;

    run_dag_tests();
    run_queue_tests();
    run_memory_tests();
    run_permission_and_invariant_tests();
    run_runtime_engine_integration_test();

    std::cout << "==========================================================" << std::endl;
    std::cout << "   ALL 5 TEST SUITES PASSED WITH 100% SUCCESS RATE!      " << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
