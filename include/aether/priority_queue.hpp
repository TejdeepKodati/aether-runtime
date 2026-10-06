#pragma once

#include <string>
#include <vector>
#include <queue>
#include <functional>
#include <chrono>
#include <cmath>
#include <iostream>
#include "aether/sync.hpp"

namespace aether {

enum class PriorityLevel {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

struct QueueTask {
    std::string id;
    std::string task_name;
    PriorityLevel priority{PriorityLevel::Normal};
    std::string payload;
    int retry_count{0};
    int max_retries{3};
    int base_backoff_ms{100};
    uint64_t enqueue_time_ms{0};
    uint64_t execute_after_ms{0}; // For delayed execution / backoff
    std::function<bool(const std::string&)> run_fn;

    bool operator<(const QueueTask& other) const {
        if (priority != other.priority) {
            return static_cast<int>(priority) < static_cast<int>(other.priority); // Higher priority first
        }
        return enqueue_time_ms > other.enqueue_time_ms; // FIFO among same priority
    }
};

struct DeadLetterItem {
    std::string id;
    std::string task_name;
    std::string payload;
    int failed_attempts{0};
    std::string last_error;
    uint64_t dropped_at_ms{0};
};

class PriorityTaskQueue {
public:
    PriorityTaskQueue() = default;

    static uint64_t current_time_ms() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
    }

    void enqueue(QueueTask task) {
        LockGuard lock(mtx_);
        if (task.enqueue_time_ms == 0) {
            task.enqueue_time_ms = current_time_ms();
        }
        if (task.execute_after_ms == 0) {
            task.execute_after_ms = task.enqueue_time_ms;
        }
        heap_.push(task);
        total_enqueued_++;
    }

    bool dequeue(QueueTask& task) {
        LockGuard lock(mtx_);
        if (heap_.empty()) {
            return false;
        }

        uint64_t now = current_time_ms();
        // Check if top task's backoff delay has elapsed
        if (heap_.top().execute_after_ms > now) {
            return false;
        }

        task = heap_.top();
        heap_.pop();
        return true;
    }

    // Handles execution with automated exponential backoff and DLQ routing
    // Time Complexity: O(log N) heap insert/remove
    bool process_next_task() {
        QueueTask task;
        if (!dequeue(task)) {
            return false;
        }

        bool success = false;
        std::string err_msg;
        try {
            if (task.run_fn) {
                success = task.run_fn(task.payload);
            } else {
                success = true;
            }
        } catch (const std::exception& e) {
            success = false;
            err_msg = e.what();
        } catch (...) {
            success = false;
            err_msg = "Unknown internal fault";
        }

        if (success) {
            LockGuard lock(mtx_);
            total_completed_++;
            return true;
        } else {
            task.retry_count++;
            if (task.retry_count <= task.max_retries) {
                // Exponential backoff: backoff = base * 2^(retry_count - 1)
                uint64_t backoff = task.base_backoff_ms * static_cast<uint64_t>(std::pow(2, task.retry_count - 1));
                task.execute_after_ms = current_time_ms() + backoff;
                {
                    LockGuard lock(mtx_);
                    heap_.push(task);
                    total_retried_++;
                }
            } else {
                // Route to Dead Letter Queue (DLQ)
                LockGuard lock(mtx_);
                dead_letter_queue_.push_back({
                    task.id,
                    task.task_name,
                    task.payload,
                    task.retry_count,
                    err_msg.empty() ? "Max retries exceeded" : err_msg,
                    current_time_ms()
                });
                total_dlq_++;
            }
            return false;
        }
    }

    size_t size() const {
        LockGuard lock(const_cast<FastMutex&>(mtx_));
        return heap_.size();
    }

    size_t dlq_size() const {
        LockGuard lock(const_cast<FastMutex&>(mtx_));
        return dead_letter_queue_.size();
    }

    std::vector<DeadLetterItem> get_dlq_items() const {
        LockGuard lock(const_cast<FastMutex&>(mtx_));
        return dead_letter_queue_;
    }

    size_t total_enqueued() const { return total_enqueued_; }
    size_t total_completed() const { return total_completed_; }
    size_t total_retried() const { return total_retried_; }

private:
    std::priority_queue<QueueTask> heap_;
    std::vector<DeadLetterItem> dead_letter_queue_;
    mutable FastMutex mtx_;

    size_t total_enqueued_{0};
    size_t total_completed_{0};
    size_t total_retried_{0};
    size_t total_dlq_{0};
};

} // namespace aether
