#pragma once

#include <string>
#include <nlohmann/json.hpp>

#include "agent/metrics_collector.hpp"
#include "agent/plan.hpp"
#include "agent/result.hpp"
#include "agent/session_manager.hpp"
#include "agent/tool_registry.hpp"
#include "agent/trace_logger.hpp"

namespace agent {

struct ExecutionResult {
    bool success{false};  // 执行是否成功，默认 false，只有全部步骤通过才会被设为 true
    nlohmann::json output{nlohmann::json::object()};  // 最终的执行输出（比如计算结果 7），默认是空的 JSON 对象 {}
    std::string error;  //错误信息，默认是空字符串 ""，出错时才会被填入具体内容
    nlohmann::json trace{nlohmann::json::object()};  //执行轨迹，记录每一步做了什么，用于调试
    nlohmann::json metrics{nlohmann::json::object()};  //性能指标，比如每一步耗时多少毫秒
};

class ToolExecutor {
public:
    // 构造函数，注入四个依赖组件
    ToolExecutor(
        ToolRegistry& registry,           // 工具注册表，用来查找工具
        SessionManager& session_manager,   // 会话管理器，管理跨步骤的共享状态
        TraceLogger& trace_logger,         // 轨迹日志器，记录执行过程
        MetricsCollector& metrics_collector // 指标收集器，记录耗时等性能数据
    );

    // 执行整个计划，返回最终结果
    ExecutionResult run(const Plan& plan);

private:
    // 执行单个步骤，返回该步骤的结果
    ToolResult executeOneStep(
        const PlanStep& step,              // 当前要执行的步骤
        const std::string& session_id,     // 所属会话的 ID
        nlohmann::json& step_trace         // 该步骤的轨迹记录（引用，会被写入）
    );

private:
    ToolRegistry& registry_;               // 工具注册表的引用
    SessionManager& session_manager_;      // 会话管理器的引用
    TraceLogger& trace_logger_;            // 轨迹日志器的引用
    MetricsCollector& metrics_collector_;  // 指标收集器的引用
};

} // namespace agent
