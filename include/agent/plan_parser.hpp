#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "agent/plan.hpp"

namespace agent {

class PlanParser {
public:
    Plan parseFile(const std::string& file_path) const;
    Plan parseJson(const nlohmann::json& json_plan) const;
};

} // namespace agent
