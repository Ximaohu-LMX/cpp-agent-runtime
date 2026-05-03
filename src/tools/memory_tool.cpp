#include "tools/memory_tool.hpp"

namespace tools {

// 返回工具名，ToolRegistry 用这个名字注册和查找
std::string MemoryTool::name() const {
    return "memory";
}

agent::ToolResult MemoryTool::execute(
    const nlohmann::json& input,
    agent::ToolContext& context
) {
    // ==========================================
    // 输入校验：必须有 key 字段且为字符串
    // ==========================================
    if (!input.contains("key") || !input["key"].is_string()) {
        return agent::ToolResult::fail("memory input.key must be a string");
    }

    const std::string key = input["key"].get<std::string>();
    const std::string op = input.value("op", "set");  // 默认操作为 set

    // ==========================================
    // GET 操作：从 session 中读取指定 key 的值
    // ==========================================
    if (op == "get") {
        auto value = context.session_manager.getValue(context.session_id, key);

        if (!value.has_value()) {  // key 不存在
            return agent::ToolResult::fail("memory key not found: " + key);
        }

        return agent::ToolResult::ok({
            {"op", "get"},
            {"key", key},
            {"value", value.value()}
        });
    }

    // ==========================================
    // SET 操作：向 session 中写入值，值有三种来源
    // ==========================================
    if (op == "set") {
        nlohmann::json value;

        if (input.contains("value_from_step")) {
            // 新增：来源一：value_from_step，指定的上一步依赖数据，用于并行
            if (!input["value_from_step"].is_string()) {
                return agent::ToolResult::fail("value_from_step must be a string");
            }
            const std::string step_id = input["value_from_step"].get<std::string>();

            auto v = context.session_manager.getValue(
                context.session_id,
                "output." + step_id   // 原"_last_output"
            );
            // 没有值报错
            if (!v.has_value()) {
                return agent::ToolResult::fail(
                    "no output found for step: " + step_id
                );
            }
            value = v.value();

        } else if (input.contains("value")) {
            // 来源二：直接从 input 中指定值
            value = input["value"];
        } else {
            // 两种来源都没有，报错
            return agent::ToolResult::fail(
                "memory set requires input.value or input.value_from_previous=true"
            );
        }

        // 写入 session
        context.session_manager.setValue(context.session_id, key, value);

        return agent::ToolResult::ok({
            {"op", "set"},
            {"key", key},
            {"value", value}
        });
    }

    // ==========================================
    // 不支持的操作类型
    // ==========================================
    return agent::ToolResult::fail("unsupported memory op: " + op);
}

} // namespace tools