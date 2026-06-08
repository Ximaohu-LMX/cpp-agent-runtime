#include "tools/rank_tool.hpp"

#include <algorithm>
#include <unordered_set>

namespace tools {

    std::string RankTool::name() const {
        return "rank";
    }

    agent::ToolResult RankTool::execute(
        const nlohmann::json& input,
        agent::ToolContext& context
    ) {
        // 阶段 1：参数校验，从上游 step 读 candidates
        if (!input.contains("candidates_from_step") ||
            !input["candidates_from_step"].is_string()) {
            return agent::ToolResult::fail(
                "rank requires input.candidates_from_step (string)"
            );
        }
        const std::string step_id = input["candidates_from_step"].get<std::string>();

        const auto step_output = context.session_manager.getValue(
            context.session_id, "output." + step_id);
        if (!step_output.has_value() ||
            !step_output.value().contains("candidates")) {
            return agent::ToolResult::fail(
                "rank: missing candidates from step " + step_id);
        }

        // 阶段 2：取偏好用于"特征加分"
        const auto preference =
            context.preference_manager.getPreferenceJson(context.session_id);

        std::unordered_set<std::string> positive_tags;
        if (preference.contains("positive_tags")) {
            for (const auto& t : preference["positive_tags"]) {
                positive_tags.insert(t.get<std::string>());
            }
        }
        std::unordered_set<std::string> preferred_categories;
        if (preference.contains("preferred_categories")) {
            for (const auto& c : preference["preferred_categories"]) {
                preferred_categories.insert(c.get<std::string>());
            }
        }

        // 阶段 3：在召回基础分上叠加偏好特征
        // 每命中一个 positive_tag +0.3，命中类别 +0.2，多源加成（sources 数 * 0.1）
        nlohmann::json candidates = step_output.value()["candidates"];
        for (auto& cand : candidates) {
            double base = cand.value("score", 0.0);

            int tag_hit = 0;
            if (cand.contains("tags") && cand["tags"].is_array()) {
                for (const auto& tag : cand["tags"]) {
                    if (positive_tags.count(tag.get<std::string>()) > 0) {
                        ++tag_hit;
                    }
                }
            }

            const double cat_hit =
                preferred_categories.count(cand.value("category", std::string{})) > 0
                    ? 1.0 : 0.0;

            const int source_count =
                (cand.contains("sources") && cand["sources"].is_array())
                    ? static_cast<int>(cand["sources"].size())
                    : 1;

            const double rank_score =
                base + tag_hit * 0.3 + cat_hit * 0.2 + (source_count - 1) * 0.1;

            cand["score"] = rank_score;
        }

        // 阶段 4：按新分数降序
        std::sort(candidates.begin(), candidates.end(),
                  [](const nlohmann::json& a, const nlohmann::json& b) {
                      return a["score"].get<double>() > b["score"].get<double>();
                  });

        return agent::ToolResult::ok({
            {"candidates", candidates},
            {"ranked_count", candidates.size()}
        });
    }

} // namespace tools
