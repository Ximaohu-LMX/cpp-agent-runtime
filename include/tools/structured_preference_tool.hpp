#pragma once

#include <string>
#include "agent/tool.hpp"
#include "agent/tag_normalizer.hpp"

namespace tools {

    /**
     * @brief 结构化偏好工具：接收 LLM 已输出的结构化偏好增量，
     *        经 TagNormalizer 做标签 / 类别标准化后返回。
     *        在推荐链路 DAG 中通常作为 PreferenceUpdateTool 的上游节点，
     *        通过 session 中的 step 输出把 normalized_preference_delta
     *        传给下游写入 PreferenceManager。
     */
    class StructuredPreferenceTool : public agent::Tool {
    public:
        /**
         * @help 返回工具的注册名，供 ToolRegistry 与 Plan.steps[].tool 字段匹配。
         * @return 固定字符串 "structured_preference"
         */
        std::string name() const override;

        /**
         * @help 执行一次结构化偏好的标准化处理。
         *       流程：
         *         1. 校验 input 中存在 preference_delta 字段，否则返回失败
         *         2. 调用 TagNormalizer 对 tags / categories 做别名 / 同义词归一
         *         3. 将归一后的偏好放入 normalized_preference_delta 字段返回
         *       本工具只做无状态的纯计算，不读写 SessionManager / PreferenceManager，
         *       因此入参 context 当前未使用（保留以满足 Tool 抽象接口）。
         * @param input   工具输入 JSON，必须包含：
         *                  - preference_delta: 对象，可含
         *                    positive_tags / negative_tags /
         *                    preferred_categories / rejected_categories /
         *                    last_query 等字段（同 PreferenceManager 约定）
         * @param context 执行上下文（session_id / SessionManager / PreferenceManager），
         *                本工具未使用
         * @return 成功：ToolResult::ok，output 形如
         *           { "normalized_preference_delta": { ... } }
         *         失败：ToolResult::fail，error 描述失败原因
         *           （如缺失 preference_delta 或归一过程中抛异常）
         */
        agent::ToolResult execute(
            const nlohmann::json& input,
            agent::ToolContext& context
        ) override;

    private:
        /// 标签标准化器：负责中文别名/同义词到标准标签的映射
        agent::TagNormalizer normalizer_;
    };

} // namespace tools