#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace agent {

struct PlanStep {
    std::string id;
    std::string tool;
    nlohmann::json input{nlohmann::json::object()};

    int timeout_ms{1000};
    int max_retries{0};
    std::string fallback_tool;
};

struct Plan {
    std::string session_id;
    std::vector<PlanStep> steps;
};

} // namespace agent
