#include "agent/tool_executor.hpp"

#include <chrono>
#include <exception>
#include <spdlog/spdlog.h>

namespace agent {

namespace {

std::uint64_t nowMs() {
    const auto now = std::chrono::steady_clock::now(); // 获取当前时刻。 steady_clock 是单调递增的时钟，不会受系统时间调整影响，适合做计时。
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            // 把这个时间段转换成毫秒单位。
            now.time_since_epoch() // 从时钟的起点到现在经过了多久，得到一个时间段（duration）
        ).count() //  取出毫秒数的原始数值
    );
}

} // namespace

ToolExecutor::ToolExecutor(
    ToolRegistry& registry,
    SessionManager& session_manager,
    TraceLogger& trace_logger,
    MetricsCollector& metrics_collector
)
    : registry_(registry),
      session_manager_(session_manager),
      trace_logger_(trace_logger),
      metrics_collector_(metrics_collector) {}

/*
    run() 开始
      ├─ 初始化 session、trace
      ├─ for 每个 step:
      │    └─ executeOneStep()
      │         ├─ for 每次尝试 (1 到 max_retries+1):
      │         │    ├─ 查工具 → 找不到就 break
      │         │    ├─ 执行工具
      │         │    ├─ 检查超时
      │         │    └─ 成功就 return，失败继续重试
      │         └─ 全部失败 → 尝试 fallback 工具
      │              └─ 成功就 return，失败就返回错误
      │    ├─ 步骤成功 → 存结果到 session，继续下一步
      │    └─ 步骤失败 → 记录错误，break 跳出循环
      ├─ 计算总耗时
      ├─ 记录 metrics、写入 trace 日志
      └─ 返回 ExecutionResult
 */

ExecutionResult ToolExecutor::run(const Plan& plan) {

    //===== 1.初始化 =====
    ExecutionResult execution_result;

    const auto run_start = nowMs(); // 返回当前时间的毫秒数

    session_manager_.createSessionIfNotExists(plan.session_id);

    nlohmann::json trace;
    trace["session_id"] = plan.session_id;
    trace["status"] = "running";
    trace["steps"] = nlohmann::json::array();

    session_manager_.appendEvent(plan.session_id, {
        {"type", "run_started"}
    });

    nlohmann::json last_output = nlohmann::json::object();

    // ===== 2.每一步循环 ======
    for (const auto& step : plan.steps) {
        nlohmann::json step_trace;
        ToolResult step_result = executeOneStep(step, plan.session_id, step_trace); //每步执行

        trace["steps"].push_back(step_trace);

        // 步骤失败时：把错误信息记录下来，往 session 追加 `step_failed` 事件，立刻 break 跳出循环，后续步骤不再执行。
        if (!step_result.success) {
            execution_result.success = false;
            execution_result.error = step_result.error;
            execution_result.output = last_output;

            session_manager_.appendEvent(plan.session_id, {
                {"type", "step_failed"},
                {"step_id", step.id},
                {"tool", step.tool},
                {"error", step_result.error}
            });

            break;
        }

        //步骤成功时：把输出存到 `last_output`，同时写入 SessionManager 的 `_last_output` 键
        // 下一步会使用 `value_from_previous` 取到上一步结果
        // 追加 `step_succeeded` 事件，继续下一步。
        last_output = step_result.output;
        session_manager_.setValue(plan.session_id, "_last_output", last_output); // 共享结果

        session_manager_.appendEvent(plan.session_id, {
            {"type", "step_succeeded"},
            {"step_id", step.id},
            {"tool", step.tool},
            {"output", step_result.output}
        });

        execution_result.success = true;
        execution_result.output = last_output; //json记录
    }

    // ====== 3.计算总耗时，记录 metrics、写入 trace 日志 =======
    const auto run_latency_ms = nowMs() - run_start;

    trace["status"] = execution_result.success ? "success" : "failed";
    trace["total_latency_ms"] = run_latency_ms;

    if (!execution_result.error.empty()) {
        trace["error"] = execution_result.error;
    }

    session_manager_.appendEvent(plan.session_id, {
        {"type", "run_finished"},
        {"success", execution_result.success},
        {"latency_ms", run_latency_ms}
    });

    metrics_collector_.recordRun(execution_result.success, run_latency_ms); // 记录成功状态和运行时间
    trace_logger_.logTrace(trace);

    execution_result.trace = trace;
    execution_result.metrics = metrics_collector_.snapshot();

    return execution_result;
}

ToolResult ToolExecutor::executeOneStep(
    const PlanStep& step,
    const std::string& session_id,
    nlohmann::json& step_trace
) {
    // ==========================================
    // 第一部分：初始化 trace 记录
    // ==========================================
    step_trace["step_id"] = step.id;
    step_trace["tool"] = step.tool;
    step_trace["status"] = "running";
    step_trace["max_retries"] = step.max_retries;
    step_trace["timeout_ms"] = step.timeout_ms;
    step_trace["attempts"] = nlohmann::json::array();

    ToolResult final_result = ToolResult::fail("step not executed");

    const int max_attempts = step.max_retries + 1;

    // ==========================================
    // 第二部分：重试循环 —— 最多尝试 max_attempts 次
    // ==========================================
    for (int attempt = 1; attempt <= max_attempts; ++attempt) {
        nlohmann::json attempt_trace;
        attempt_trace["attempt"] = attempt;

        // ---------- 2.1 查找工具 ----------
        auto tool = registry_.getTool(step.tool);
        if (!tool) {
            final_result = ToolResult::fail("tool not found: " + step.tool);
            attempt_trace["status"] = "failed";
            attempt_trace["error"] = final_result.error;
            step_trace["attempts"].push_back(attempt_trace);
            break;  // 工具不存在，重试也没用，直接跳出
        }

        // ---------- 2.2 执行工具 ----------
        const auto start_ms = nowMs();

        try {
            ToolContext context{
                .session_id = session_id,
                .session_manager = session_manager_
            };
            final_result = tool->execute(step.input, context);
        } catch (const std::exception& ex) {
            final_result = ToolResult::fail(ex.what());
        } catch (...) {
            final_result = ToolResult::fail("unknown tool execution error");
        }

        // ---------- 2.3 超时检测（事后检测） ----------
        // 意义：用于评测场景下，工具执行完但是超时，在实际生产环境中这个延迟是不可接受的，所以评测时应该判定为失败。

        const auto latency_ms = nowMs() - start_ms;

        if (latency_ms > static_cast<std::uint64_t>(step.timeout_ms)) {
            final_result = ToolResult::fail(
                "tool timeout: " + step.tool +
                ", latency_ms=" + std::to_string(latency_ms)
            );
        }

        // ---------- 2.4 记录本次尝试的指标和 trace ----------
        metrics_collector_.recordToolCall(
            step.tool,
            final_result.success,
            latency_ms
        );

        attempt_trace["latency_ms"] = latency_ms;
        attempt_trace["status"] = final_result.success ? "success" : "failed";

        if (final_result.success) {
            attempt_trace["output"] = final_result.output;
        } else {
            attempt_trace["error"] = final_result.error;
        }

        step_trace["attempts"].push_back(attempt_trace);

        // ---------- 2.5 成功则立刻返回，失败则继续重试 ----------
        if (final_result.success) {
            step_trace["status"] = "success";
            step_trace["output"] = final_result.output;
            return final_result;
        }

        spdlog::warn(
            "step failed, step_id={}, tool={}, attempt={}, error={}",
            step.id, step.tool, attempt, final_result.error
        );
    }// attempt 循环结束

    // ==========================================
    // 第三部分：Fallback —— 所有重试都失败后，尝试备用工具
    // ==========================================
    if (!final_result.success && !step.fallback_tool.empty()) {
        nlohmann::json fallback_trace;
        fallback_trace["tool"] = step.fallback_tool;

        // ---------- 3.1 查找备用工具 ----------
        auto fallback = registry_.getTool(step.fallback_tool);
        if (!fallback) {
            fallback_trace["status"] = "failed";
            fallback_trace["error"] = "fallback tool not found";
        } else {
            // ---------- 3.2 执行备用工具 ----------
            const auto start_ms = nowMs();

            try {
                ToolContext context{
                    .session_id = session_id,
                    .session_manager = session_manager_
                };
                final_result = fallback->execute(step.input, context);
            } catch (const std::exception& ex) {
                final_result = ToolResult::fail(ex.what());
            } catch (...) {
                final_result = ToolResult::fail("unknown fallback execution error");
            }

            // ---------- 3.3 记录备用工具的指标和 trace ----------
            const auto latency_ms = nowMs() - start_ms;

            metrics_collector_.recordToolCall(
                step.fallback_tool,
                final_result.success,
                latency_ms
            );

            fallback_trace["latency_ms"] = latency_ms;
            fallback_trace["status"] = final_result.success ? "success" : "failed";

            if (final_result.success) {
                fallback_trace["output"] = final_result.output;
            } else {
                fallback_trace["error"] = final_result.error;
            }
        }

        step_trace["fallback"] = fallback_trace;

        // ---------- 3.4 备用工具成功则返回 ----------
        if (final_result.success) {
            step_trace["status"] = "success";
            step_trace["output"] = final_result.output;
            return final_result;
        }
    }

    // ==========================================
    // 第四部分：彻底失败 —— 重试和 fallback 都没救回来
    // ==========================================
    step_trace["status"] = "failed";
    step_trace["error"] = final_result.error;

    return final_result;
}

} // namespace agent
