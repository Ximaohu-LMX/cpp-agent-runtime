#pragma once

#include <string>
#include "agent/tool.hpp"

namespace tools {

    /**
     * @brief 精排（占位）：用 negative_tags / rejected_categories 做硬过滤，
     *        命中即剔除（不降权）。剩余候选保持原顺序输出。
     *
     * input:
     *   { "candidates_from_step": "rank" }
     */
    class RerankTool : public agent::Tool {
    public:
        std::string name() const override;

        agent::ToolResult execute(
            const nlohmann::json& input,
            agent::ToolContext& context
        ) override;
    };

} // namespace tools
