#include "tools/vector_recall_tool.hpp"

#include <algorithm>
#include <unordered_set>

#include "agent/item_catalog.hpp"

namespace tools {

    std::string VectorRecallTool::name() const {
        return "vector_recall";
    }

    agent::ToolResult VectorRecallTool::execute(
        const nlohmann::json& input,
        agent::ToolContext& context
    ) {
        (void) input;

        // 阶段 1：从 PreferenceManager 读偏好（单一事实源）
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

        // 阶段 2：对每个物料计算"伪向量相似度"
        // 打分 = 正向标签命中比例 * 0.7 + 类别命中 * 0.3
        nlohmann::json candidates = nlohmann::json::array();

        for (const auto& item : agent::getItemCatalog()) {
            int matched = 0;
            for (const auto& tag : item.tags) {
                if (positive_tags.count(tag) > 0) ++matched;
            }

            const double tag_score =
                positive_tags.empty()
                    ? 0.0
                    : static_cast<double>(matched) /
                          static_cast<double>(positive_tags.size());

            const double category_score =
                preferred_categories.count(item.category) > 0 ? 1.0 : 0.0;

            const double score = tag_score * 0.7 + category_score * 0.3;

            // 阶段 3：低于阈值的物料不进召回结果
            if (score <= 0.0) continue;

            candidates.push_back({
                {"id", item.id},
                {"title", item.title},
                {"tags", item.tags},
                {"category", item.category},
                {"score", score},
                {"sources", nlohmann::json::array({"vector"})}
            });
        }

        // 阶段 4：按 score 降序，便于上层观察
        std::sort(candidates.begin(), candidates.end(),
                  [](const nlohmann::json& a, const nlohmann::json& b) {
                      return a["score"].get<double>() > b["score"].get<double>();
                  });

        return agent::ToolResult::ok({
            {"candidates", candidates},
            {"source", "vector"}
        });
    }

} // namespace tools
