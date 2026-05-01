#pragma once

#include <string>
#include "agent/tool.hpp"

namespace tools {

    class EchoTool : public agent::Tool {
    public:
        std::string name() const override;

        agent::ToolResult execute(
            const nlohmann::json& input,
            agent::ToolContext& context
        ) override;
    };

} // namespace tools
