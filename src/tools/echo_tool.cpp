#include "tools/echo_tool.hpp"

#include <cctype>
#include <cmath>
#include <stdexcept>
#include <string>

namespace tools {

// ==========================================
// EchoTool 的接口实现
// ==========================================

// 返回工具名，用于 ToolRegistry 注册和查找
std::string EchoTool::name() const {
    return "echo";
}

// 执行入口：从 input 中取出表达式字符串，原样输出
agent::ToolResult EchoTool::execute(
    const nlohmann::json& input,
    agent::ToolContext& context
) {
    (void) context;  // 计算器不需要会话数据，显式标记未使用，避免编译器警告

    // 输入校验：必须有 expression 字段且为字符串
    if (!input.contains("message") || !input["message"].is_string()) {
        return agent::ToolResult::fail("echo input.message must be a string");
    }

    const std::string msg = input["message"].get<std::string>();

    return agent::ToolResult::ok({
            {"message", msg}
        });

}

} // namespace tools