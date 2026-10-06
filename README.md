# AetherRuntime: Deterministic Autonomous Agent Runtime & Invariant Engine

[![C++17/20](https://img.shields.io/badge/C%2B%2B-17%2F20-blue.svg)](https://isocpp.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build Status](https://img.shields.io/badge/Build-Passing-brightgreen.svg)]()

> High-performance, concurrent C++ systems runtime powering deterministic task planning, execution, hierarchical company memory, capability-based security gating, and invariant verification for autonomous AI employees.

Built by **[Tejdeep Kodati](https://github.com/TejdeepKodati)**.

---

## 🏛️ Architectural Overview

Autonomous AI agents in enterprise environments face strict reliability requirements: non-deterministic execution loops, unverified tool outputs, cascading task failures, and unsafe actions cannot be permitted in production. 

**AetherRuntime** solves this by treating agent execution as a deterministic distributed systems problem:

```
                  ┌────────────────────────────────────────┐
                  │       Agent Task Decomposition         │
                  └───────────────────┬────────────────────┘
                                      │
                         [ Kahn's Algorithm O(V+E) ]
                                      ▼
                  ┌────────────────────────────────────────┐
                  │     Topological Execution Layers       │
                  │   [Batch 0] ──► [Batch 1] ──► [Batch 2]│
                  └───────────────────┬────────────────────┘
                                      │
                 ┌────────────────────┴────────────────────┐
                 ▼                                         ▼
   ┌───────────────────────────┐             ┌───────────────────────────┐
   │    PermissionGuard        │             │   CheckpointManager       │
   │  • Token Bucket O(1)      │             │  • O(1) Transactional     │
   │  • Risk Scoring (HITL)    │             │    Snapshot Stack         │
   └─────────────┬─────────────┘             └─────────────┬─────────────┘
                 │                                         │
                 ▼                                         │
   ┌───────────────────────────┐                           │
   │   Isolated Tool Execution │                           │
   └─────────────┬─────────────┘                           │
                 │                                         │
                 ▼                                         │
   ┌───────────────────────────┐                           │
   │   InvariantEngine         │                           │
   │  • Pre/Post Conditions    │                           │
   │  • Secret Leak Guard      │                           ▼
   │  • Schema Invariants      │ ──[ Violation ]──► State Rollback Stack
   └─────────────┬─────────────┘
                 │ [ Passed ]
                 ▼
   ┌───────────────────────────────────────────────────────┐
   │            Hierarchical Company Memory                │
   │  • L1 Working Cache: Custom Doubly-Linked LRU O(1)    │
   │  • L2 Entity Ontology: Prefix Trie O(L)               │
   │  • L3 Semantic Recall: Cosine Similarity Vector k-NN  │
   └───────────────────────────────────────────────────────┘
```

---

## 🚀 Key Engineering & DSA Highlights

### 1. Directed Acyclic Graph (DAG) Task Scheduler
- **Algorithm**: Kahn's Topological Sort with cycle detection ($O(V + E)$).
- Computes parallel execution layers allowing independent tasks to execute concurrently while strictly respecting upstream dependencies.
- Rejects circular dependencies immediately with descriptive fault tracing.
- Supports dynamic graph re-planning upon intermediate step failures.

### 2. Hierarchical Multi-Tiered Memory Architecture
- **L1 Working Memory**: Custom thread-safe **LRU Cache** implemented with a doubly-linked list and hash table ($O(1)$ Get, $O(1)$ Put, $O(1)$ Eviction).
- **L2 Company Ontology**: **Prefix Trie** enabling instant $O(L)$ key resolution for departmental policy lookups, runbook rules, and access control scopes.
- **L3 Semantic Memory**: Dense vector memory index computing **Cosine Similarity** ($\frac{\mathbf{A} \cdot \mathbf{B}}{\|\mathbf{A}\| \|\mathbf{B}\|}$) and Top-$K$ min-heap selection ($O(N \log K)$).

### 3. Capability-Based Security & Token Bucket Rate Limiter
- Granular `ActionRisk` classification: `Safe`, `ReadOnly`, `Sensitive`, `Destructive`.
- Strict **Human-in-the-Loop (HITL)** gating: Any destructive tool invocation automatically halts execution pending human clearance.
- Continuous-time Token Bucket algorithm ($O(1)$ amortized) preventing runaway LLM recursive loops.

### 4. Invariant Engine & Transactional Rollback
- Pre-condition and post-condition assertion engine.
- Prevents secret leakage (API keys, credentials), unclosed payload schemas, and unapproved destructive database commands (`DROP TABLE`, `TRUNCATE`).
- Checkpoint stack snapshots system state before each mutation; if an invariant is violated, an $O(1)$ transactional rollback restores prior state.

### 5. Priority Task Queue with Exponential Backoff & DLQ
- Concurrent priority binary heap ($O(\log N)$ push/pop) with custom comparator.
- Automated exponential backoff ($2^{k-1} \times \text{base}$) for transient failures.
- Dead Letter Queue (DLQ) for inspecting and replaying exhausted jobs.

---

## ⚡ Performance & Benchmarks

Measured on GCC C++17 with `-O2` release optimizations:

| Benchmark | Complexity | Measured Throughput / Latency |
|---|---|---|
| **L1 LRU Cache Lookups** | $O(1)$ | **8,659,508 ops/sec** (100k in 11.55 ms) |
| **L1 LRU Cache Insertions** | $O(1)$ | **2,288,172 ops/sec** (100k in 43.70 ms) |
| **1,000-Node DAG Compilation** | $O(V+E)$ | **< 150 µs** |
| **L3 Vector Search ($k=5$, 1000 Docs)** | $O(N \cdot D)$ | **14.33 µs / query** |
| **Checkpoint State Rollback** | $O(1)$ | **< 5 µs** |

---

## 🛠️ Local Build & Testing Instructions

### Prerequisites
- Modern C++ compiler supporting C++17/20 (`g++` >= 8.0, `clang++`, or `MSVC`)
- CMake >= 3.15 (optional)
- Node.js >= 18 (for web dashboard)

### 1. Compile and Run Test Suite
```bash
# Using g++ directly
g++ -std=c++17 -O2 -I include tests/test_suite.cpp -o test_suite
./test_suite
```

### 2. Run High-Throughput Benchmarks
```bash
g++ -std=c++17 -O2 -I include benchmarks/benchmark.cpp -o benchmark
./benchmark
```

### 3. Run Interactive Runtime CLI
```bash
g++ -std=c++17 -O2 -I include src/main.cpp -o aether_runtime
./aether_runtime
```

### 4. Run Interactive Web Dashboard
```bash
npm install
npm run dev
```

---

## 🌐 Live Web Deployment
Experience the interactive DAG scheduler, memory tier inspector, and invariant engine live:
- **Deployed on Vercel**: `https://aether-runtime.vercel.app`
- **GitHub Repository**: [https://github.com/TejdeepKodati/aether-runtime](https://github.com/TejdeepKodati/aether-runtime)

---

## 📄 License
MIT © Tejdeep Kodati
