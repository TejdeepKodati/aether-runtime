#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <chrono>
#include <algorithm>
#include "aether/sync.hpp"

namespace aether {

enum class ActionRisk {
    Safe = 0,         // e.g. health check, formatting
    ReadOnly = 1,     // e.g. reading files, querying GET endpoints
    Sensitive = 2,    // e.g. updating non-critical database rows, posting Slack message
    Destructive = 3   // e.g. deleting files, initiating refunds, schema drops (Requires HITL)
};

inline std::string risk_to_string(ActionRisk r) {
    switch (r) {
        case ActionRisk::Safe: return "Safe";
        case ActionRisk::ReadOnly: return "ReadOnly";
        case ActionRisk::Sensitive: return "Sensitive";
        case ActionRisk::Destructive: return "Destructive (HITL Required)";
    }
    return "Unknown";
}

enum class AgentRole {
    Viewer,
    Operator,
    Admin,
    SystemAgent
};

struct PermissionCheckResult {
    bool allowed{false};
    bool requires_hitl{false};
    std::string reason;
    ActionRisk risk_level{ActionRisk::Safe};
};

// Algorithmic rate limiter: Token Bucket Algorithm
// Time Complexity: O(1) amortized, Space Complexity: O(1) per bucket
class TokenBucketRateLimiter {
public:
    TokenBucketRateLimiter(double capacity, double refill_rate_per_sec)
        : capacity_(capacity),
          tokens_(capacity),
          refill_rate_(refill_rate_per_sec),
          last_refill_(std::chrono::steady_clock::now()) {}

    bool consume(double tokens = 1.0) {
        LockGuard lock(mtx_);
        refill();
        if (tokens_ >= tokens) {
            tokens_ -= tokens;
            return true;
        }
        return false;
    }

    double current_tokens() {
        LockGuard lock(mtx_);
        refill();
        return tokens_;
    }

private:
    void refill() {
        auto now = std::chrono::steady_clock::now();
        double elapsed_sec = std::chrono::duration<double>(now - last_refill_).count();
        tokens_ = std::min(capacity_, tokens_ + elapsed_sec * refill_rate_);
        last_refill_ = now;
    }

    double capacity_;
    double tokens_;
    double refill_rate_;
    std::chrono::steady_clock::time_point last_refill_;
    FastMutex mtx_;
};

class PermissionGuard {
public:
    PermissionGuard(double rate_capacity = 50.0, double rate_refill_per_sec = 20.0)
        : rate_limiter_(rate_capacity, rate_refill_per_sec) {
        // Default role permissions
        role_max_risk_[AgentRole::Viewer] = ActionRisk::ReadOnly;
        role_max_risk_[AgentRole::Operator] = ActionRisk::Sensitive;
        role_max_risk_[AgentRole::Admin] = ActionRisk::Destructive;
        role_max_risk_[AgentRole::SystemAgent] = ActionRisk::Sensitive;
    }

    void register_tool_policy(const std::string& tool_name, ActionRisk risk, const std::string& allowed_scope) {
        LockGuard lock(mtx_);
        tool_risk_map_[tool_name] = risk;
        tool_scope_map_[tool_name] = allowed_scope;
    }

    PermissionCheckResult evaluate_action(
        AgentRole role,
        const std::string& tool_name,
        const std::string& target_resource,
        bool human_approval_granted = false
    ) {
        // 1. Rate limiting check
        if (!rate_limiter_.consume(1.0)) {
            return {false, false, "Rate limit exceeded. Token bucket depleted.", ActionRisk::Safe};
        }

        LockGuard lock(mtx_);
        ActionRisk risk = ActionRisk::ReadOnly;
        auto it = tool_risk_map_.find(tool_name);
        if (it != tool_risk_map_.end()) {
            risk = it->second;
        }

        // 2. Destructive actions strictly require Human-in-the-Loop (HITL) approval
        if (risk == ActionRisk::Destructive) {
            if (!human_approval_granted) {
                return {false, true, "Action is classified as Destructive. Pausing execution for Human-in-the-Loop approval.", risk};
            }
        }

        // 3. RBAC risk ceiling validation
        ActionRisk max_allowed = role_max_risk_[role];
        if (static_cast<int>(risk) > static_cast<int>(max_allowed) && !human_approval_granted) {
            return {false, false, "Action risk (" + risk_to_string(risk) + ") exceeds role maximum authorization.", risk};
        }

        return {true, false, "Authorized and verified by PermissionGuard policy.", risk};
    }

private:
    std::unordered_map<AgentRole, ActionRisk> role_max_risk_;
    std::unordered_map<std::string, ActionRisk> tool_risk_map_;
    std::unordered_map<std::string, std::string> tool_scope_map_;
    TokenBucketRateLimiter rate_limiter_;
    FastMutex mtx_;
};

} // namespace aether
