#pragma once

#include <string>
#include "agent/tool.hpp"

namespace tools {

    /**
     * @brief 响应构建（占位）：截取 top_n 条候选作为最终展示项，
     *        附带文案 summary。链路终端节点。
     *
     * input:
     *   { "candidates_from_step": "rerank", "top_n": 3 }
     */
    class ResponseBuilderTool : public agent::Tool {
    public:
        std::string name() const override;

        agent::ToolResult execute(
            const nlohmann::json& input,
            agent::ToolContext& context
        ) override;
    };

} // namespace tools
