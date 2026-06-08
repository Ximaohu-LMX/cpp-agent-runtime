#pragma once

#include <string>
#include "agent/tool.hpp"

namespace tools {

    /**
     * @brief 关键词召回（占位）：基于 last_query 做子串匹配，
     *        辅以正向标签精确命中，输出 source="keyword" 的候选列表。
     */
    class KeywordRecallTool : public agent::Tool {
    public:
        std::string name() const override;

        agent::ToolResult execute(
            const nlohmann::json& input,
            agent::ToolContext& context
        ) override;
    };

} // namespace tools
