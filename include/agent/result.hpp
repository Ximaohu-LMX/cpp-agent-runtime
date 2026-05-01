#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace agent {

struct ToolResult {
    bool success{false};
    nlohmann::json output{nlohmann::json::object()};
    std::string error;

    static ToolResult ok(nlohmann::json output) {
        ToolResult result;
        result.success = true;
        result.output = std::move(output);
        return result;
    }

    static ToolResult fail(std::string error) {
        ToolResult result;
        result.success = false;
        result.error = std::move(error);
        return result;
    }
};

} // namespace agent
