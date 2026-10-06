#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <regex>
#include <iostream>

namespace aether {

struct InvariantCheck {
    std::string name;
    std::string description;
    std::function<bool(const std::string& output, std::string& violation_detail)> predicate;
};

struct VerificationReport {
    bool passed{true};
    std::string task_id;
    std::vector<std::string> passed_invariants;
    std::vector<std::string> failed_invariants;
    std::string details;
};

class InvariantEngine {
public:
    InvariantEngine() {
        // Built-in safety invariants
        register_invariant("no_system_secrets", "Ensures tool output never leaks raw API keys or passwords",
            [](const std::string& output, std::string& violation) {
                if (output.find("AIzaSy") != std::string::npos ||
                    output.find("sk-live-") != std::string::npos ||
                    output.find("AWS_SECRET_ACCESS_KEY") != std::string::npos) {
                    violation = "Critical security leak: API key pattern detected in output";
                    return false;
                }
                return true;
            });

        register_invariant("valid_json_boundary", "Ensures JSON-like payload has balanced curly brackets",
            [](const std::string& output, std::string& violation) {
                if (output.empty()) return true;
                int balance = 0;
                for (char c : output) {
                    if (c == '{') balance++;
                    if (c == '}') balance--;
                    if (balance < 0) {
                        violation = "Malformed output: premature closing brace in payload";
                        return false;
                    }
                }
                if (balance != 0) {
                    violation = "Malformed output: unclosed opening brace in payload";
                    return false;
                }
                return true;
            });

        register_invariant("sql_destructive_guard", "Guarantees no unapproved destructive SQL statements like DROP TABLE",
            [](const std::string& output, std::string& violation) {
                std::string upper = output;
                for (auto& c : upper) c = toupper(c);
                if (upper.find("DROP TABLE") != std::string::npos || upper.find("TRUNCATE TABLE") != std::string::npos) {
                    violation = "Dangerous database mutation detected without human bypass authorization";
                    return false;
                }
                return true;
            });
    }

    void register_invariant(const std::string& name, const std::string& desc,
                            std::function<bool(const std::string&, std::string&)> pred) {
        invariants_.push_back({name, desc, pred});
    }

    VerificationReport verify_output(const std::string& task_id, const std::string& output) const {
        VerificationReport report;
        report.task_id = task_id;

        for (const auto& inv : invariants_) {
            std::string violation;
            if (inv.predicate(output, violation)) {
                report.passed_invariants.push_back(inv.name);
            } else {
                report.passed = false;
                report.failed_invariants.push_back(inv.name + ": " + violation);
            }
        }

        if (!report.passed) {
            report.details = "Invariant verification failed with " +
                             std::to_string(report.failed_invariants.size()) + " violation(s).";
        } else {
            report.details = "All " + std::to_string(report.passed_invariants.size()) +
                             " verification invariants passed successfully.";
        }

        return report;
    }

private:
    std::vector<InvariantCheck> invariants_;
};

} // namespace aether
