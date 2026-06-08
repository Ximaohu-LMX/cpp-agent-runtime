#include "tools/hot_recall_tool.hpp"

#include <algorithm>
#include <unordered_set>

#include "agent/item_catalog.hpp"

namespace tools {

    std::string HotRecallTool::name() const {
        return "hot_recall";
    }

    agent::ToolResult HotRecallTool::execute(
        const nlohmann::json& input,
        agent::ToolContext& context
    ) {
        // 默认取热度前 5
        const int top_n = input.value("top_n", 5);

        const auto preference =
            context.preference_manager.getPreferenceJson(context.session_id);

        std::unordered_set<std::string> preferred_categories;
        if (preference.contains("preferred_categories")) {
            for (const auto& c : preference["preferred_categories"]) {
                preferred_categories.insert(c.get<std::string>());
            }
        }

        // 阶段 1：克隆物料库，做"偏好类别加权"
        // 在偏好类别里 score = popularity；否则 popularity * 0.5
        struct Scored {
            const agent::CatalogItem* item;
            double score;
        };

        std::vector<Scored> scored;
        scored.reserve(agent::getItemCatalog().size());

        for (const auto& item : agent::getItemCatalog()) {
            const double weight =
                preferred_categories.count(item.category) > 0 ? 1.0 : 0.5;
            scored.push_back({&item, item.popularity * weight});
        }

        // 阶段 2：按 score 降序，截 top_n
        std::sort(scored.begin(), scored.end(),
                  [](const Scored& a, const Scored& b) {
                      return a.score > b.score;
                  });

        if (static_cast<int>(scored.size()) > top_n) {
            scored.resize(top_n);
        }

        nlohmann::json candidates = nlohmann::json::array();
        for (const auto& s : scored) {
            candidates.push_back({
                {"id", s.item->id},
                {"title", s.item->title},
                {"tags", s.item->tags},
                {"category", s.item->category},
                {"score", s.score},
                {"sources", nlohmann::json::array({"hot"})}
            });
        }

        return agent::ToolResult::ok({
            {"candidates", candidates},
            {"source", "hot"}
        });
    }

} // namespace tools
