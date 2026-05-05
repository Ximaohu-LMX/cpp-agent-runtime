#include "tools/structured_preference_tool.hpp"

namespace tools {

    std::string StructuredPreferenceTool::name() const {
        return "structured_preference";
    }

    agent::ToolResult StructuredPreferenceTool::execute(
        const nlohmann::json& input,
        agent::ToolContext& context
    ) {
        (void) context;

        // 阶段 1：接收上游 LLM 已经输出好的结构化偏好
        if (!input.contains("preference_delta")) {
            return agent::ToolResult::fail(
                "structured_preference requires input.preference_delta"
            );
        }

        try {
            // 阶段 2：标准化标签和类别
            const auto normalized =
                normalizer_.normalizePreferenceDelta(input["preference_delta"]);

            // 阶段 3：返回标准化后的偏好增量
            // 后续 PreferenceUpdateTool 会把它写入 PreferenceManager。
            return agent::ToolResult::ok({
                {"normalized_preference_delta", normalized}
            });
        } catch (const std::exception& ex) {
            return agent::ToolResult::fail(ex.what());
        }
    }

} // namespace tools