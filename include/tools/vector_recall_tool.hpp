#pragma once

#include <string>
#include "agent/tool.hpp"

namespace tools {

    /**
     * @brief 向量召回（占位）：模拟 embedding 召回，
     *        以正向标签命中数 + 类别偏好作为相似度，输出 source="vector" 的候选列表。
     */
    class VectorRecallTool : public agent::Tool {
    public:
        std::string name() const override;

        agent::ToolResult execute(
            const nlohmann::json& input,
            agent::ToolContext& context
        ) override;
    };

} // namespace tools
