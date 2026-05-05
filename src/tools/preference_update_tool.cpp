#include "tools/preference_update_tool.hpp"

namespace tools {

    // 返回工具名，用于 ToolRegistry 注册和查找
    std::string PreferenceUpdateTool::name() const {
        return "preference_update";
    }

    // ==========================================
    // 将标准化后的偏好增量写入 PreferenceManager
    // 偏好数据有两种来源：从前置步骤读取，或直接传入
    // ==========================================
    agent::ToolResult PreferenceUpdateTool::execute(
        const nlohmann::json& input,
        agent::ToolContext& context
    ) {
        nlohmann::json preference_delta;

        // ---- 来源一：从指定 step 的输出中读取 ----
        // 典型场景：上一步 StructuredPreferenceTool 做完标准化，
        // 本步通过 step_id 取其输出中的 normalized_preference_delta
        if (input.contains("preference_from_step")) {
            if (!input["preference_from_step"].is_string()) {
                return agent::ToolResult::fail("preference_from_step must be a string");
            }

            const std::string step_id = input["preference_from_step"].get<std::string>();

            // 从 SessionManager 中按 step_id 取输出
            auto step_output = context.session_manager.getValue(
                context.session_id,
                "output." + step_id
            );

            if (!step_output.has_value()) {
                return agent::ToolResult::fail("no output found for step: " + step_id);
            }

            if (!step_output.value().contains("normalized_preference_delta")) {
                return agent::ToolResult::fail(
                    "step output does not contain normalized_preference_delta: " + step_id
                );
            }

            preference_delta = step_output.value()["normalized_preference_delta"];
        }
        // ---- 来源二：直接传入，用于单元测试或简单场景 ----
        else if (input.contains("preference_delta")) {
            preference_delta = input["preference_delta"];
        }
        else {
            return agent::ToolResult::fail(
                "preference_update requires preference_from_step or preference_delta"
            );
        }

        // ---- 写入 PreferenceManager，执行偏好合并/去重/覆盖 ----
        context.preference_manager.updatePreference(
            context.session_id,
            preference_delta
        );

        // 返回更新后的完整偏好，方便调试和评测
        const auto current_preference =
            context.preference_manager.getPreferenceJson(context.session_id);

        return agent::ToolResult::ok({
            {"updated", true},
            {"preference", current_preference}
        });
    }

} // namespace tools