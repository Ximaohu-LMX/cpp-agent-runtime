#include "agent/metrics_collector.hpp"

namespace agent {

void MetricsCollector::recordToolCall(
    const std::string& tool_name,
    bool success,
    std::uint64_t latency_ms
) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto& metric = tool_metrics_[tool_name];
    metric.call_count++;
    metric.total_latency_ms += latency_ms;

    if (success) {
        metric.success_count++;
    } else {
        metric.error_count++;
    }
}

void MetricsCollector::recordRun(bool success, std::uint64_t latency_ms) {
    std::lock_guard<std::mutex> lock(mutex_);

    run_count_++;
    run_total_latency_ms_ += latency_ms;

    if (success) {
        run_success_count_++;
    } else {
        run_error_count_++;
    }
}

nlohmann::json MetricsCollector::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);

    nlohmann::json result;
    result["runs"]["count"] = run_count_;
    result["runs"]["success_count"] = run_success_count_;
    result["runs"]["error_count"] = run_error_count_;
    result["runs"]["avg_latency_ms"] =
        run_count_ == 0 ? 0 : run_total_latency_ms_ / run_count_;

    result["tools"] = nlohmann::json::object();

    for (const auto& [tool_name, metric] : tool_metrics_) {
        result["tools"][tool_name]["call_count"] = metric.call_count;
        result["tools"][tool_name]["success_count"] = metric.success_count;
        result["tools"][tool_name]["error_count"] = metric.error_count;
        result["tools"][tool_name]["avg_latency_ms"] =
            metric.call_count == 0 ? 0 : metric.total_latency_ms / metric.call_count;
    }

    return result;
}

} // namespace agent
