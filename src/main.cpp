#include <iostream>
#include <memory>
#include <string>

#include <spdlog/spdlog.h>

#include "agent/metrics_collector.hpp"
#include "agent/plan_parser.hpp"
#include "agent/session_manager.hpp"
#include "agent/tool_executor.hpp"
#include "agent/tool_registry.hpp"
#include "agent/trace_logger.hpp"
#include "agent/preference_manager.hpp"

#include "tools/calculator_tool.hpp"
#include "tools/memory_tool.hpp"
#include "tools/echo_tool.hpp"
#include "tools/structured_preference_tool.hpp"
#include "tools/preference_update_tool.hpp"
#include "tools/vector_recall_tool.hpp"
#include "tools/keyword_recall_tool.hpp"
#include "tools/hot_recall_tool.hpp"
#include "tools/merge_candidates_tool.hpp"
#include "tools/rank_tool.hpp"
#include "tools/rerank_tool.hpp"
#include "tools/response_builder_tool.hpp"

int main(int argc, char** argv) {
    // 获取计划文件路径
    const std::string plan_file =
        argc >= 2 ? argv[1] : "configs/sample_plan.json";

    try {
        // 初始化日志
        spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v"); //格式例如 [14:30:05] [info] Agent Runtime starting
        spdlog::info("Agent Runtime starting");
        spdlog::info("Using plan file: {}", plan_file);

        // 注册工具，手动有几个写几个
        agent::ToolRegistry registry;
        registry.registerTool(std::make_shared<tools::CalculatorTool>());
        registry.registerTool(std::make_shared<tools::MemoryTool>());
        registry.registerTool(std::make_shared<tools::EchoTool>());
        registry.registerTool(std::make_shared<tools::StructuredPreferenceTool>());
        registry.registerTool(std::make_shared<tools::PreferenceUpdateTool>());
        registry.registerTool(std::make_shared<tools::VectorRecallTool>());
        registry.registerTool(std::make_shared<tools::KeywordRecallTool>());
        registry.registerTool(std::make_shared<tools::HotRecallTool>());
        registry.registerTool(std::make_shared<tools::MergeCandidatesTool>());
        registry.registerTool(std::make_shared<tools::RankTool>());
        registry.registerTool(std::make_shared<tools::RerankTool>());
        registry.registerTool(std::make_shared<tools::ResponseBuilderTool>());

        // 创建辅助组件
        agent::SessionManager session_manager;   // 管理会话状态（跨步骤共享数据）
        agent::TraceLogger trace_logger("logs"); // 记录执行轨迹（每步做了什么） 参数：log_dir
        agent::MetricsCollector metrics_collector; // 收集性能指标（耗时等）
        agent::PreferenceManager preference_manager;

        // 解析 JSON 计划
        agent::PlanParser parser;
        agent::Plan plan = parser.parseFile(plan_file);

        // 执行计划
        // ToolExecutor 拿到所有依赖（工具注册表、会话管理、日志、指标），然后按顺序执行 plan 里的每个 step。
        // 它会根据每个 step 的 tool 字段去 registry 查工具，传入 input，处理超时和重试。
        agent::ToolExecutor executor(
            registry,
            session_manager,
            preference_manager,
            trace_logger,
            metrics_collector
        );

        // 输出结果
        agent::ExecutionResult result = executor.run(plan);

        std::cout << "\n=== Execution Result ===\n";
        std::cout << "success: " << std::boolalpha << result.success << "\n";  //std::boolalpha 让 bool 值以英文单词输出，而不是数字

        if (!result.error.empty()) {
            std::cout << "error: " << result.error << "\n";
        }

        std::cout << "\noutput:\n" << result.output.dump(2) << "\n";
//        std::cout << "\ntrace:\n" << result.trace.dump(2) << "\n";
//        std::cout << "\nmetrics:\n" << result.metrics.dump(2) << "\n";
//
//        std::cout << "\nsession:\n"
//                  << session_manager.dumpSession(plan.session_id).dump(2)
//                  << "\n";
        std::cout << "\npreference:\n"
          << preference_manager.getPreferenceJson(plan.session_id).dump(2)
          << "\n";

        return result.success ? 0 : 1;
    } catch (const std::exception& ex) {
        spdlog::error("fatal error: {}", ex.what());
        return 1;
    }
}
