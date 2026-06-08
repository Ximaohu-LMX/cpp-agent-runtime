#pragma once

#include <string>
#include "agent/tool.hpp"

namespace tools {

    /**
     * @brief 热度召回（占位）：按物料 popularity 排序，
     *        若用户有 preferred_categories 则在该集合内优先，
     *        输出 source="hot" 的候选列表。
     */
    class HotRecallTool : public agent::Tool {
    public:
        std::string name() const override;

        agent::ToolResult execute(
            const nlohmann::json& input,
            agent::ToolContext& context
        ) override;
    };

} // namespace tools
