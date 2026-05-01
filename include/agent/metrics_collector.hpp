#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace agent {

struct ToolMetrics {
    std::uint64_t call_count{0};
    std::uint64_t success_count{0};
    std::uint64_t error_count{0};
    std::uint64_t total_latency_ms{0};
};

class MetricsCollector {
public:
    void recordToolCall(
        const std::string& tool_name,
        bool success,
        std::uint64_t latency_ms
    );

    void recordRun(bool success, std::uint64_t latency_ms);

    nlohmann::json snapshot() const;

private:
    mutable std::mutex mutex_;

    std::uint64_t run_count_{0};
    std::uint64_t run_success_count_{0};
    std::uint64_t run_error_count_{0};
    std::uint64_t run_total_latency_ms_{0};

    std::unordered_map<std::string, ToolMetrics> tool_metrics_;
};

} // namespace agent
