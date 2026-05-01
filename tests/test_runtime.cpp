#include <memory>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "agent/metrics_collector.hpp"
#include "agent/plan_parser.hpp"
#include "agent/session_manager.hpp"
#include "agent/tool_executor.hpp"
#include "agent/tool_registry.hpp"
#include "agent/trace_logger.hpp"

#include "tools/calculator_tool.hpp"
#include "tools/memory_tool.hpp"

TEST(PlanParserTest, ParseValidPlan) {
    nlohmann::json json_plan = {
        {"session_id", "test_session"},
        {"steps", {
            {
                {"id", "calc_1"},
                {"tool", "calculator"},
                {"input", {
                    {"expression", "1 + 2 * 3"}
                }}
            }
        }}
    };

    agent::PlanParser parser;
    agent::Plan plan = parser.parseJson(json_plan);

    ASSERT_EQ(plan.session_id, "test_session");
    ASSERT_EQ(plan.steps.size(), 1);
    EXPECT_EQ(plan.steps[0].id, "calc_1");
    EXPECT_EQ(plan.steps[0].tool, "calculator");
}

TEST(ToolRegistryTest, RegisterAndFindTool) {
    agent::ToolRegistry registry;
    registry.registerTool(std::make_shared<tools::CalculatorTool>());

    EXPECT_TRUE(registry.hasTool("calculator"));
    EXPECT_NE(registry.getTool("calculator"), nullptr);
    EXPECT_EQ(registry.getTool("unknown"), nullptr);
}

TEST(RuntimeTest, ExecuteCalculatorAndMemoryPlan) {
    agent::ToolRegistry registry;
    registry.registerTool(std::make_shared<tools::CalculatorTool>());
    registry.registerTool(std::make_shared<tools::MemoryTool>());

    agent::SessionManager session_manager;
    agent::TraceLogger trace_logger("logs");
    agent::MetricsCollector metrics_collector;

    nlohmann::json json_plan = {
        {"session_id", "test_session"},
        {"steps", {
            {
                {"id", "calc_1"},
                {"tool", "calculator"},
                {"input", {
                    {"expression", "1 + 2 * 3"}
                }}
            },
            {
                {"id", "save_1"},
                {"tool", "memory"},
                {"input", {
                    {"key", "last_result"},
                    {"value_from_previous", true}
                }}
            },
            {
                {"id", "get_1"},
                {"tool", "memory"},
                {"input", {
                    {"op", "get"},
                    {"key", "last_result"}
                }}
            }
        }}
    };

    agent::PlanParser parser;
    agent::Plan plan = parser.parseJson(json_plan);

    agent::ToolExecutor executor(
        registry,
        session_manager,
        trace_logger,
        metrics_collector
    );

    agent::ExecutionResult result = executor.run(plan);

    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.output.contains("value"));
    ASSERT_TRUE(result.output["value"].contains("result"));
    EXPECT_DOUBLE_EQ(result.output["value"]["result"].get<double>(), 7.0);
}

TEST(RuntimeTest, UnknownToolShouldFail) {
    agent::ToolRegistry registry;
    registry.registerTool(std::make_shared<tools::CalculatorTool>());

    agent::SessionManager session_manager;
    agent::TraceLogger trace_logger("logs");
    agent::MetricsCollector metrics_collector;

    nlohmann::json json_plan = {
        {"session_id", "test_session"},
        {"steps", {
            {
                {"id", "bad_1"},
                {"tool", "unknown_tool"},
                {"input", nlohmann::json::object()}
            }
        }}
    };

    agent::PlanParser parser;
    agent::Plan plan = parser.parseJson(json_plan);

    agent::ToolExecutor executor(
        registry,
        session_manager,
        trace_logger,
        metrics_collector
    );

    agent::ExecutionResult result = executor.run(plan);

    ASSERT_FALSE(result.success);
    ASSERT_NE(result.error.find("tool not found"), std::string::npos);
}
