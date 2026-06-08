#pragma once

#include <string>
#include "agent/tool.hpp"

namespace tools {

    /**
     * @brief 粗排（占位）：在合并后的候选上叠加偏好特征，
     *        对每条 candidate 重新打分并降序排列。
     *
     * input:
     *   { "candidates_from_step": "merge_candidates" }
     */
    class RankTool : public agent::Tool {
    public:
        std::string name() const override;

        agent::ToolResult execute(
            const nlohmann::json& input,
            agent::ToolContext& context
        ) override;
    };

} // namespace tools
