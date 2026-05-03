#include "agent/tool_executor.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <functional>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
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


// ==========================================
// 验证计划中步骤的依赖关系是否合法
// 在执行前调用，发现问题直接抛异常，阻止执行
// ==========================================
void ToolExecutor::validatePlanDependencies(const Plan& plan) const {

    // ==========================================
    // 第一阶段：构建步骤索引，检查 id 合法性
    // ==========================================
    std::unordered_map<std::string, const PlanStep*> step_map;

    for (const auto& step : plan.steps) {
        // id 不能为空
        if (step.id.empty()) {
            throw std::runtime_error("step id cannot be empty");
        }
        // id 不能重复
        if (step_map.count(step.id) > 0) {
            throw std::runtime_error("duplicate step id: " + step.id);
        }
        // 存入索引：id -> 步骤指针
        step_map[step.id] = &step;
    }

    // ==========================================
    // 第二阶段：检查每个步骤的依赖是否合法
    // ==========================================
    for (const auto& step : plan.steps) {
        for (const auto& dep : step.depends_on) {
            // 依赖的步骤必须存在
            if (step_map.count(dep) == 0) {
                throw std::runtime_error(
                    "step " + step.id + " depends on unknown step: " + dep
                );
            }
            // 不能依赖自己
            if (dep == step.id) {
                throw std::runtime_error(
                    "step cannot depend on itself: " + step.id
                );
            }
        }
    }

    // ==========================================
    // 第三阶段：用 DFS 检测循环依赖
    // 比如 A 依赖 B，B 依赖 C，C 又依赖 A，这种情况无法执行
    // ==========================================
    enum class VisitState {
        Unvisited,  // 还没访问过
        Visiting,   // 正在访问中（在当前 DFS 路径上）
        Visited     // 已经访问完毕（确认无环）
    };

    // 初始化所有步骤为未访问
    std::unordered_map<std::string, VisitState> states;
    for (const auto& step : plan.steps) {
        states[step.id] = VisitState::Unvisited;
    }

    // DFS 递归函数
    std::function<void(const std::string&)> dfs =
        [&](const std::string& step_id) {
            auto state = states[step_id];

            // 如果当前节点正在访问中又被访问到，说明有环
            if (state == VisitState::Visiting) {
                throw std::runtime_error(
                    "cycle detected in plan dependencies near step: " + step_id
                );
            }

            // 已经确认无环的节点，跳过
            if (state == VisitState::Visited) {
                return;
            }

            // 标记为正在访问
            states[step_id] = VisitState::Visiting;

            // 递归检查所有依赖
            const auto* step = step_map.at(step_id);
            for (const auto& dep : step->depends_on) {
                dfs(dep);
            }

            // 所有依赖都检查完了，标记为已完成
            states[step_id] = VisitState::Visited;
        };

    // 对每个步骤启动 DFS
    for (const auto& step : plan.steps) {
        dfs(step.id);
    }
}

std::vector<std::vector<const PlanStep*>>
ToolExecutor::buildExecutionLayers(const Plan& plan) const {
    // 阶段 1：建立 step_id -> step 的映射，方便后续通过 id 找到 step
    std::unordered_map<std::string, const PlanStep*> step_map;

    for (const auto& step : plan.steps) {
        step_map[step.id] = &step;
    }

    // 阶段 2：构建入度表和邻接表
    // indegree[step_id] 表示当前 step 还有多少依赖没有完成
    // graph[dep] 表示 dep 完成后，可以解锁哪些后续 step
    std::unordered_map<std::string, int> indegree;
    std::unordered_map<std::string, std::vector<std::string>> graph;

    for (const auto& step : plan.steps) {
        indegree[step.id] = 0;
    }

    for (const auto& step : plan.steps) {
        for (const auto& dep : step.depends_on) {
            graph[dep].push_back(step.id);
            indegree[step.id]++;
        }
    }

    // 阶段 3：找出第一层，也就是没有任何依赖的 step
    std::vector<std::string> current_layer;

    for (const auto& step : plan.steps) {
        if (indegree[step.id] == 0) {
            current_layer.push_back(step.id);
        }
    }

    std::vector<std::vector<const PlanStep*>> layers;
    std::size_t visited_count = 0;

    // 阶段 4：逐层拓扑遍历
    // 当前层执行完成后，减少后继节点的入度
    // 入度变成 0 的节点进入下一层
    while (!current_layer.empty()) {
        std::vector<const PlanStep*> layer_steps;
        std::vector<std::string> next_layer;

        for (const auto& step_id : current_layer) {
            layer_steps.push_back(step_map.at(step_id));
            visited_count++;

            for (const auto& next_step_id : graph[step_id]) {
                indegree[next_step_id]--;

                if (indegree[next_step_id] == 0) {
                    next_layer.push_back(next_step_id);
                }
            }
        }

        layers.push_back(std::move(layer_steps));
        current_layer = std::move(next_layer);
    }

    // 阶段 5：理论上 validatePlanDependencies 已经检查过环
    // 这里再加一层保护，防止未来修改校验逻辑后出现漏检
    if (visited_count != plan.steps.size()) {
        throw std::runtime_error("failed to build execution layers: cycle may exist");
    }

    return layers;
}

/*
    run() 开始
      ├─ 验证依赖关系（id 合法性、依赖存在性、循环依赖检测）
      ├─ 初始化 session、trace
      ├─ 构建拓扑分层（buildExecutionLayers）
      ├─ for 每个 Layer:
      │    ├─ for Layer 内每个 step:
      │    │    └─ executeOneStep()
      │    │         ├─ for 每次尝试 (1 到 max_retries+1):
      │    │         │    ├─ 查工具 → 找不到就 break
      │    │         │    ├─ 执行工具
      │    │         │    ├─ 检查超时
      │    │         │    └─ 成功就 return，失败继续重试
      │    │         └─ 全部失败 → 尝试 fallback 工具
      │    │              └─ 成功就 return，失败就返回错误
      │    │    ├─ 步骤成功 → 存结果到 session（_last_output + output.step_id）
      │    │    └─ 步骤失败 → 记录错误，标记 should_stop
      │    └─ should_stop → break 跳出 Layer 循环
      ├─ 计算总耗时
      ├─ 记录 metrics、写入 trace 日志
      └─ 返回 ExecutionResult
 */

ExecutionResult ToolExecutor::run(const Plan& plan) {

    // ==========================================
    // 第一阶段：初始化
    // ==========================================

    // 验证依赖关系：id 合法性、依赖存在性、循环依赖检测
    validatePlanDependencies(plan);

    ExecutionResult execution_result;
    const auto run_start = nowMs();

    // 确保 session 存在，初始化 trace 结构
    session_manager_.createSessionIfNotExists(plan.session_id);

    nlohmann::json trace;
    trace["session_id"] = plan.session_id;
    trace["status"] = "running";
    trace["steps"] = nlohmann::json::array();
    trace["layers"] = nlohmann::json::array();
    trace["dag"] = nlohmann::json::object();

    session_manager_.appendEvent(plan.session_id, {{"type", "run_started"}});

    // 根据 depends_on 构建拓扑分层（DAG）
    // 例如：Layer 0: [calc_a, calc_b, echo_test]  Layer 1: [save_result]  Layer 2: [read_result]
    const auto layers = buildExecutionLayers(plan); // std::vector<std::vector<const PlanStep*>>

    // 分层信息写入trace["dag"]
    trace["dag"]["layer_count"] = layers.size();
    trace["dag"]["layer_sizes"] = nlohmann::json::array();
    trace["dag"]["edges"] = nlohmann::json::array();
    for (const auto& layer : layers) {
        trace["dag"]["layer_sizes"].push_back(layer.size());
        for(const auto& p : layer) {
            if(!p->depends_on.empty()) {
                // 非空，写入边 依赖到自己
                for(const auto& dep : p->depends_on) {
                    trace["dag"]["edges"].push_back({{"from", dep}, {"to", p->id}});
                }
            }
        }
    }


    nlohmann::json last_output = nlohmann::json::object();
    bool should_stop = false;

    // ==========================================
    // 第二阶段：按 Layer 顺序执行
    // ==========================================
    // 同一 Layer 内的步骤互不依赖，当前为顺序执行
    // 将来做并行只需把 Layer 内的 for 改为线程池即可

    for (std::size_t layer_index = 0; layer_index < layers.size(); ++layer_index) {
        nlohmann::json layer_trace;
        layer_trace["layer_index"] = layer_index;
        layer_trace["steps"] = nlohmann::json::array();

        spdlog::info("Executing layer {}, step_count={}", layer_index, layers[layer_index].size());

        for (const PlanStep* step_ptr : layers[layer_index]) {
            const PlanStep& step = *step_ptr;

            // ---- 执行单个步骤 ----
            nlohmann::json step_trace;
            ToolResult step_result = executeOneStep(step, plan.session_id, step_trace);

            trace["steps"].push_back(step_trace);       // 兼容旧的平铺 trace
            layer_trace["steps"].push_back(step_trace);  // 新的分层 trace

            // ---- 步骤失败：记录错误，标记停止 ----
            if (!step_result.success) {
                execution_result.success = false;
                execution_result.error = step_result.error;
                execution_result.output = last_output;

                session_manager_.appendEvent(plan.session_id, {
                    {"type", "step_failed"},
                    {"layer_index", layer_index},
                    {"step_id", step.id},
                    {"tool", step.tool},
                    {"error", step_result.error}
                });

                should_stop = true;
                break;
            }

            // ---- 步骤成功：保存输出 ----
            last_output = step_result.output;
            // session_manager_.setValue(plan.session_id, "_last_output", last_output);  //旧的 value_from_previous,不需要了
            session_manager_.setValue(plan.session_id, "output." + step.id, step_result.output); // 按 step id 存储，为并行做准备

            session_manager_.appendEvent(plan.session_id, {
                {"type", "step_succeeded"},
                {"layer_index", layer_index},
                {"step_id", step.id},
                {"tool", step.tool},
                {"output", step_result.output}
            });

            execution_result.success = true;
            execution_result.output = last_output;
        }

        trace["layers"].push_back(layer_trace);

        if (should_stop) break;  // 当前 Layer 有步骤失败，不再执行后续 Layer
    }

    // ==========================================
    // 第三阶段：收尾，记录总耗时和指标
    // ==========================================
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

    metrics_collector_.recordRun(execution_result.success, run_latency_ms);
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
