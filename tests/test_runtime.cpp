#include <memory>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "agent/metrics_collector.hpp"
#include "agent/plan_parser.hpp"
#include "agent/preference_manager.hpp"
#include "agent/session_manager.hpp"
#include "agent/tool_executor.hpp"
#include "agent/tool_registry.hpp"
#include "agent/trace_logger.hpp"

#include "tools/calculator_tool.hpp"
#include "tools/memory_tool.hpp"
#include "tools/echo_tool.hpp"

TEST(RuntimeTest, ExecuteDagPlanWithValueFromStep) {
    agent::ToolRegistry registry;
    registry.registerTool(std::make_shared<tools::EchoTool>());
    registry.registerTool(std::make_shared<tools::CalculatorTool>());
    registry.registerTool(std::make_shared<tools::MemoryTool>());

    agent::SessionManager session_manager;
    agent::TraceLogger trace_logger("logs");
    agent::MetricsCollector metrics_collector;
    agent::PreferenceManager preference_manager;

    nlohmann::json json_plan = {
        {"session_id", "s1"},
        {"steps", {
            {
                {"id", "echo_test"},
                {"tool", "echo"},
                {"depends_on", nlohmann::json::array()},
                {"timeout_ms", 1000},
                {"max_retries", 0},
                {"input", {
                    {"message", "hello agent"}
                }}
            },
            {
                {"id", "calc_a"},
                {"tool", "calculator"},
                {"depends_on", nlohmann::json::array()},
                {"timeout_ms", 1000},
                {"max_retries", 1},
                {"input", {
                    {"expression", "1 + 1"}
                }}
            },
            {
                {"id", "calc_b"},
                {"tool", "calculator"},
                {"depends_on", nlohmann::json::array()},
                {"timeout_ms", 1000},
                {"max_retries", 1},
                {"input", {
                    {"expression", "2 + 2"}
                }}
            },
            {
                {"id", "save_result"},
                {"tool", "memory"},
                {"depends_on", {"calc_b"}},
                {"timeout_ms", 1000},
                {"max_retries", 0},
                {"input", {
                    {"key", "last_result"},
                    {"value_from_step", "calc_b"}
                }}
            },
            {
                {"id", "read_result"},
                {"tool", "memory"},
                {"depends_on", {"save_result"}},
                {"timeout_ms", 1000},
                {"max_retries", 0},
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
        preference_manager,
        trace_logger,
        metrics_collector
    );

    agent::ExecutionResult result = executor.run(plan);

    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.output.contains("value"));
    ASSERT_TRUE(result.output["value"].contains("result"));

    EXPECT_DOUBLE_EQ(result.output["value"]["result"].get<double>(), 4.0);

    ASSERT_TRUE(result.trace.contains("dag"));
    EXPECT_EQ(result.trace["dag"]["layer_count"].get<int>(), 3);

    ASSERT_TRUE(result.trace.contains("layers"));
    ASSERT_EQ(result.trace["layers"].size(), 3);

    auto saved = session_manager.getValue("s1", "last_result");
    ASSERT_TRUE(saved.has_value());
    ASSERT_TRUE(saved.value().contains("result"));
    EXPECT_DOUBLE_EQ(saved.value()["result"].get<double>(), 4.0);
}
