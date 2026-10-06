#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cmath>
#include <queue>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include "aether/sync.hpp"

namespace aether {

// ============================================================================
// L1: Thread-Safe LRU Cache (Doubly-Linked List + Hash Map)
// Time Complexity: O(1) Get, O(1) Put, O(1) Eviction
// ============================================================================
class LRUCache {
public:
    explicit LRUCache(size_t capacity = 128) : capacity_(capacity) {
        head_ = std::make_unique<Node>("", "");
        tail_ = std::make_unique<Node>("", "");
        head_->next = tail_.get();
        tail_->prev = head_.get();
    }

    bool get(const std::string& key, std::string& value) {
        LockGuard lock(mtx_);
        auto it = map_.find(key);
        if (it == map_.end()) {
            return false;
        }
        Node* node = it->second;
        move_to_head(node);
        value = node->value;
        return true;
    }

    void put(const std::string& key, const std::string& value) {
        LockGuard lock(mtx_);
        auto it = map_.find(key);
        if (it != map_.end()) {
            Node* node = it->second;
            node->value = value;
            move_to_head(node);
            return;
        }

        if (map_.size() >= capacity_) {
            Node* lru = tail_->prev;
            remove_node(lru);
            map_.erase(lru->key);
            delete lru;
        }

        Node* new_node = new Node(key, value);
        insert_after_head(new_node);
        map_[key] = new_node;
    }

    size_t size() const {
        LockGuard lock(const_cast<FastMutex&>(mtx_));
        return map_.size();
    }

    ~LRUCache() {
        Node* curr = head_->next;
        while (curr != tail_.get() && curr != nullptr) {
            Node* next = curr->next;
            delete curr;
            curr = next;
        }
    }

private:
    struct Node {
        std::string key;
        std::string value;
        Node* prev{nullptr};
        Node* next{nullptr};
        Node(std::string k, std::string v) : key(std::move(k)), value(std::move(v)) {}
    };

    void remove_node(Node* node) {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

    void insert_after_head(Node* node) {
        node->next = head_->next;
        node->prev = head_.get();
        head_->next->prev = node;
        head_->next = node;
    }

    void move_to_head(Node* node) {
        remove_node(node);
        insert_after_head(node);
    }

    size_t capacity_;
    std::unordered_map<std::string, Node*> map_;
    std::unique_ptr<Node> head_;
    std::unique_ptr<Node> tail_;
    mutable FastMutex mtx_;
};

// ============================================================================
// L2: Prefix Trie for Company Ontology, Policy Keys, and Entity Resolution
// Time Complexity: O(L) Insertion, O(L) Prefix Search where L is key length
// ============================================================================
class PrefixTrie {
public:
    struct TrieNode {
        std::unordered_map<char, std::unique_ptr<TrieNode>> children;
        bool is_terminal{false};
        std::string payload;
    };

    PrefixTrie() : root_(std::make_unique<TrieNode>()) {}

    void insert(const std::string& key, const std::string& payload) {
        LockGuard lock(mtx_);
        TrieNode* curr = root_.get();
        for (char c : key) {
            if (curr->children.find(c) == curr->children.end()) {
                curr->children[c] = std::make_unique<TrieNode>();
            }
            curr = curr->children[c].get();
        }
        curr->is_terminal = true;
        curr->payload = payload;
    }

    bool find(const std::string& key, std::string& payload) const {
        LockGuard lock(const_cast<FastMutex&>(mtx_));
        const TrieNode* curr = root_.get();
        for (char c : key) {
            auto it = curr->children.find(c);
            if (it == curr->children.end()) {
                return false;
            }
            curr = it->second.get();
        }
        if (curr->is_terminal) {
            payload = curr->payload;
            return true;
        }
        return false;
    }

    std::vector<std::pair<std::string, std::string>> find_by_prefix(const std::string& prefix) const {
        LockGuard lock(const_cast<FastMutex&>(mtx_));
        std::vector<std::pair<std::string, std::string>> results;
        const TrieNode* curr = root_.get();
        for (char c : prefix) {
            auto it = curr->children.find(c);
            if (it == curr->children.end()) {
                return results; // Empty
            }
            curr = it->second.get();
        }

        collect_dfs(curr, prefix, results);
        return results;
    }

private:
    void collect_dfs(const TrieNode* node, const std::string& current_prefix,
                     std::vector<std::pair<std::string, std::string>>& results) const {
        if (!node) return;
        if (node->is_terminal) {
            results.emplace_back(current_prefix, node->payload);
        }
        for (const auto& [ch, child] : node->children) {
            collect_dfs(child.get(), current_prefix + ch, results);
        }
    }

    std::unique_ptr<TrieNode> root_;
    mutable FastMutex mtx_;
};

// ============================================================================
// L3: Dense Vector Similarity Search (Cosine Similarity k-NN)
// Time Complexity: O(N * D) for N vectors of dimension D, Top-K selection O(N log K)
// ============================================================================
struct VectorDoc {
    std::string id;
    std::string text;
    std::vector<float> embedding;
};

class VectorMemoryIndex {
public:
    explicit VectorMemoryIndex(size_t dimension) : dimension_(dimension) {}

    void add_document(const std::string& id, const std::string& text, const std::vector<float>& embedding) {
        if (embedding.size() != dimension_) {
            throw std::invalid_argument("Vector embedding dimension mismatch");
        }
        LockGuard lock(mtx_);
        docs_.push_back({id, text, normalize(embedding)});
    }

    struct SearchResult {
        std::string id;
        std::string text;
        float score;
    };

    std::vector<SearchResult> search(const std::vector<float>& query_embedding, size_t top_k = 3) const {
        if (query_embedding.size() != dimension_) {
            throw std::invalid_argument("Query vector dimension mismatch");
        }
        std::vector<float> norm_query = normalize(query_embedding);

        LockGuard lock(const_cast<FastMutex&>(mtx_));
        // Min-heap to maintain Top-K results: pair<score, index>
        using ScoredItem = std::pair<float, size_t>;
        std::priority_queue<ScoredItem, std::vector<ScoredItem>, std::greater<ScoredItem>> min_heap;

        for (size_t i = 0; i < docs_.size(); ++i) {
            float sim = dot_product(norm_query, docs_[i].embedding);
            if (min_heap.size() < top_k) {
                min_heap.push({sim, i});
            } else if (sim > min_heap.top().first) {
                min_heap.pop();
                min_heap.push({sim, i});
            }
        }

        std::vector<SearchResult> results;
        while (!min_heap.empty()) {
            auto [score, idx] = min_heap.top();
            min_heap.pop();
            results.push_back({docs_[idx].id, docs_[idx].text, score});
        }
        std::reverse(results.begin(), results.end()); // Descending order
        return results;
    }

    size_t size() const {
        LockGuard lock(const_cast<FastMutex&>(mtx_));
        return docs_.size();
    }

private:
    static std::vector<float> normalize(const std::vector<float>& vec) {
        float sum_sq = 0.0f;
        for (float v : vec) sum_sq += v * v;
        float norm = std::sqrt(sum_sq);
        if (norm < 1e-9f) return vec;
        std::vector<float> result(vec.size());
        for (size_t i = 0; i < vec.size(); ++i) {
            result[i] = vec[i] / norm;
        }
        return result;
    }

    static float dot_product(const std::vector<float>& a, const std::vector<float>& b) {
        float dot = 0.0f;
        for (size_t i = 0; i < a.size(); ++i) {
            dot += a[i] * b[i];
        }
        return dot;
    }

    size_t dimension_;
    std::vector<VectorDoc> docs_;
    mutable FastMutex mtx_;
};

// Unified Hierarchical Memory Manager
class HierarchicalMemoryManager {
public:
    HierarchicalMemoryManager(size_t l1_capacity = 64, size_t l3_dim = 8)
        : l1_cache_(l1_capacity), l3_index_(l3_dim) {}

    LRUCache& l1() { return l1_cache_; }
    PrefixTrie& l2() { return l2_trie_; }
    VectorMemoryIndex& l3() { return l3_index_; }

private:
    LRUCache l1_cache_;
    PrefixTrie l2_trie_;
    VectorMemoryIndex l3_index_;
};

} // namespace aether
