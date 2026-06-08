#include "tools/rerank_tool.hpp"

#include <unordered_set>

namespace tools {

    std::string RerankTool::name() const {
        return "rerank";
    }

    agent::ToolResult RerankTool::execute(
        const nlohmann::json& input,
        agent::ToolContext& context
    ) {
        if (!input.contains("candidates_from_step") ||
            !input["candidates_from_step"].is_string()) {
            return agent::ToolResult::fail(
                "rerank requires input.candidates_from_step (string)"
            );
        }
        const std::string step_id = input["candidates_from_step"].get<std::string>();

        const auto step_output = context.session_manager.getValue(
            context.session_id, "output." + step_id);
        if (!step_output.has_value() ||
            !step_output.value().contains("candidates")) {
            return agent::ToolResult::fail(
                "rerank: missing candidates from step " + step_id);
        }

        // 阶段 1：取负反馈集合
        const auto preference =
            context.preference_manager.getPreferenceJson(context.session_id);

        std::unordered_set<std::string> negative_tags;
        if (preference.contains("negative_tags")) {
            for (const auto& t : preference["negative_tags"]) {
                negative_tags.insert(t.get<std::string>());
            }
        }
        std::unordered_set<std::string> rejected_categories;
        if (preference.contains("rejected_categories")) {
            for (const auto& c : preference["rejected_categories"]) {
                rejected_categories.insert(c.get<std::string>());
            }
        }

        // 阶段 2：硬过滤——命中负反馈（任一 tag 或 category）的候选直接剔除
        nlohmann::json kept = nlohmann::json::array();
        nlohmann::json dropped = nlohmann::json::array();

        for (const auto& cand : step_output.value()["candidates"]) {
            bool is_dropped = false;
            std::string drop_reason;

            const std::string category = cand.value("category", std::string{});
            if (rejected_categories.count(category) > 0) {
                is_dropped = true;
                drop_reason = "rejected_category:" + category;
            }

            if (!is_dropped && cand.contains("tags") && cand["tags"].is_array()) {
                for (const auto& tag : cand["tags"]) {
                    if (negative_tags.count(tag.get<std::string>()) > 0) {
                        is_dropped = true;
                        drop_reason = "negative_tag:" + tag.get<std::string>();
                        break;
                    }
                }
            }

            if (is_dropped) {
                nlohmann::json drop_record = {
                    {"id", cand.value("id", std::string{})},
                    {"reason", drop_reason}
                };
                dropped.push_back(drop_record);
            } else {
                kept.push_back(cand);
            }
        }

        return agent::ToolResult::ok({
            {"candidates", kept},
            {"kept_count", kept.size()},
            {"dropped", dropped}
        });
    }

} // namespace tools
