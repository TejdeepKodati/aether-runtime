#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <random>

#include "aether/task_dag.hpp"
#include "aether/priority_queue.hpp"
#include "aether/hierarchical_memory.hpp"
#include "aether/invariant_verifier.hpp"

using namespace aether;

void benchmark_lru_cache() {
    std::cout << "\n[BENCHMARK] L1 LRU Cache Throughput..." << std::endl;
    LRUCache cache(1000);
    const int NUM_OPS = 100000;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_OPS; ++i) {
        cache.put("key_" + std::to_string(i % 1500), "val_" + std::to_string(i));
    }
    auto mid = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_OPS; ++i) {
        std::string dummy;
        cache.get("key_" + std::to_string(i % 1500), dummy);
    }
    auto end = std::chrono::high_resolution_clock::now();

    double put_ms = std::chrono::duration<double, std::milli>(mid - start).count();
    double get_ms = std::chrono::duration<double, std::milli>(end - mid).count();

    std::cout << "  - 100,000 Puts: " << std::fixed << std::setprecision(2) << put_ms << " ms ("
              << (NUM_OPS / (put_ms / 1000.0)) << " ops/sec)" << std::endl;
    std::cout << "  - 100,000 Gets: " << std::fixed << std::setprecision(2) << get_ms << " ms ("
              << (NUM_OPS / (get_ms / 1000.0)) << " ops/sec)" << std::endl;
}

void benchmark_dag_planning() {
    std::cout << "\n[BENCHMARK] TaskDAG Topological Plan Compilation (Kahn's Algorithm)..." << std::endl;
    TaskDAG dag;
    const int NUM_NODES = 1000;

    for (int i = 0; i < NUM_NODES; ++i) {
        dag.add_node({"node_" + std::to_string(i), "Task " + std::to_string(i), "exec", "{}"});
    }

    // Create a multi-tier dependency chain
    for (int i = 0; i < NUM_NODES - 1; ++i) {
        if (i % 5 != 0) {
            dag.add_dependency("node_" + std::to_string(i), "node_" + std::to_string(i + 1));
        }
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto plan = dag.compute_execution_plan();
    auto end = std::chrono::high_resolution_clock::now();

    double time_us = std::chrono::duration<double, std::micro>(end - start).count();
    std::cout << "  - 1,000 Node DAG Compilation: " << std::fixed << std::setprecision(2)
              << time_us << " us (" << plan.size() << " parallel layers computed)" << std::endl;
}

void benchmark_vector_memory() {
    std::cout << "\n[BENCHMARK] L3 Vector Memory Cosine Similarity Search..." << std::endl;
    const size_t DIM = 16;
    const size_t NUM_DOCS = 1000;
    VectorMemoryIndex vec_idx(DIM);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    for (size_t i = 0; i < NUM_DOCS; ++i) {
        std::vector<float> emb(DIM);
        for (size_t d = 0; d < DIM; ++d) emb[d] = dist(rng);
        vec_idx.add_document("doc_" + std::to_string(i), "Enterprise policy document content " + std::to_string(i), emb);
    }

    std::vector<float> query(DIM);
    for (size_t d = 0; d < DIM; ++d) query[d] = dist(rng);

    const int NUM_QUERIES = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    for (int q = 0; q < NUM_QUERIES; ++q) {
        auto res = vec_idx.search(query, 5);
    }
    auto end = std::chrono::high_resolution_clock::now();

    double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "  - 1,000 Vector Queries over 1,000 Docs (k=5): " << std::fixed << std::setprecision(2)
              << total_ms << " ms (" << (total_ms / NUM_QUERIES * 1000.0) << " us/query)" << std::endl;
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "   AetherRuntime Performance & Latency Benchmarks        " << std::endl;
    std::cout << "==========================================================" << std::endl;

    benchmark_dag_planning();
    benchmark_lru_cache();
    benchmark_vector_memory();

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "   BENCHMARKS COMPLETED SUCCESSFULLY!                     " << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
