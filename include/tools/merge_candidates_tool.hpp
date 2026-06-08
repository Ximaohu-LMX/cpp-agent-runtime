#pragma once

#include <string>
#include "agent/tool.hpp"

namespace tools {

    /**
     * @brief 多路召回结果合并：从多个上游 step 的输出中读 candidates，
     *        按 id 去重（取 max score），保留 sources 列表（被哪几路命中）。
     *        DAG 中首次出现"多对一"汇聚节点。
     *
     * input:
     *   { "candidates_from_steps": ["vector_recall", "keyword_recall", "hot_recall"] }
     */
    class MergeCandidatesTool : public agent::Tool {
    public:
        std::string name() const override;

        agent::ToolResult execute(
            const nlohmann::json& input,
            agent::ToolContext& context
        ) override;
    };

} // namespace tools
