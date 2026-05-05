#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "agent/result.hpp"
#include "agent/session_manager.hpp"
#include "agent/preference_manager.hpp"

namespace agent {

/*
    传给工具的执行环境信息包，让工具知道自己在哪个会话里运行，并且能读写会话数据。
    step.input 是"做什么"（具体的输入数据），context 是"在什么环境下做"。
 */
struct ToolContext {
    std::string session_id;
    SessionManager& session_manager;
    PreferenceManager& preference_manager;
};

class Tool {
public:
    virtual ~Tool() = default;

    virtual std::string name() const = 0;

    virtual ToolResult execute(
        const nlohmann::json& input,
        ToolContext& context
    ) = 0;
};

} // namespace agent
