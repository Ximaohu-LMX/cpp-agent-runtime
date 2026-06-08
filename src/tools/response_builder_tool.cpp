#include "tools/response_builder_tool.hpp"

#include <sstream>

namespace tools {

    std::string ResponseBuilderTool::name() const {
        return "response_builder";
    }

    agent::ToolResult ResponseBuilderTool::execute(
        const nlohmann::json& input,
        agent::ToolContext& context
    ) {
        if (!input.contains("candidates_from_step") ||
            !input["candidates_from_step"].is_string()) {
            return agent::ToolResult::fail(
                "response_builder requires input.candidates_from_step (string)"
            );
        }
        const std::string step_id = input["candidates_from_step"].get<std::string>();
        const int top_n = input.value("top_n", 3);

        const auto step_output = context.session_manager.getValue(
            context.session_id, "output." + step_id);
        if (!step_output.has_value() ||
            !step_output.value().contains("candidates")) {
            return agent::ToolResult::fail(
                "response_builder: missing candidates from step " + step_id);
        }

        // 阶段 1：截 top_n
        nlohmann::json items = nlohmann::json::array();
        const auto& cands = step_output.value()["candidates"];
        for (std::size_t i = 0;
             i < cands.size() && static_cast<int>(i) < top_n; ++i) {
            const auto& c = cands[i];
            items.push_back({
                {"id", c.value("id", std::string{})},
                {"title", c.value("title", std::string{})},
                {"score", c.value("score", 0.0)},
                {"sources", c.value("sources", nlohmann::json::array())}
            });
        }

        // 阶段 2：拼一段简短的中文文案
        std::ostringstream oss;
        oss << "根据您的偏好，为您推荐 " << items.size() << " 条内容：";
        for (std::size_t i = 0; i < items.size(); ++i) {
            oss << "\n  " << (i + 1) << ". "
                << items[i]["title"].get<std::string>();
        }

        return agent::ToolResult::ok({
            {"items", items},
            {"summary", oss.str()},
            {"total_returned", items.size()}
        });
    }

} // namespace tools
