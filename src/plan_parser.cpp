#include "agent/plan_parser.hpp"

#include <fstream>
#include <stdexcept>

namespace agent {

Plan PlanParser::parseFile(const std::string& file_path) const {
    std::ifstream input(file_path);
    if (!input.is_open()) {
        throw std::runtime_error("failed to open plan file: " + file_path);
    }

    nlohmann::json json_plan;
    input >> json_plan;

    return parseJson(json_plan);
}

Plan PlanParser::parseJson(const nlohmann::json& json_plan) const {
    if (!json_plan.is_object()) {
        throw std::runtime_error("plan must be a JSON object");
    }

    Plan plan;
    plan.session_id = json_plan.value("session_id", "default_session");

    if (!json_plan.contains("steps") || !json_plan["steps"].is_array()) {
        throw std::runtime_error("plan.steps must be an array");
    }

    const auto& steps = json_plan["steps"];
    if (steps.empty()) {
        throw std::runtime_error("plan.steps cannot be empty");
    }

    for (std::size_t i = 0; i < steps.size(); ++i) {
        const auto& json_step = steps[i];

        if (!json_step.is_object()) {
            throw std::runtime_error("each step must be a JSON object");
        }

        if (!json_step.contains("tool") || !json_step["tool"].is_string()) {
            throw std::runtime_error("step.tool is required and must be a string");
        }

        PlanStep step;
        step.id = json_step.value("id", "step_" + std::to_string(i + 1));
        step.tool = json_step["tool"].get<std::string>();
        step.input = json_step.contains("input")
            ? json_step["input"]
            : nlohmann::json::object();

        step.timeout_ms = json_step.value("timeout_ms", 1000);
        step.max_retries = json_step.value("max_retries", 0);
        step.fallback_tool = json_step.value("fallback_tool", "");

        if (step.timeout_ms <= 0) {
            throw std::runtime_error("step.timeout_ms must be positive");
        }

        if (step.max_retries < 0) {
            throw std::runtime_error("step.max_retries cannot be negative");
        }

        plan.steps.push_back(std::move(step));
    }

    return plan;
}

} // namespace agent
