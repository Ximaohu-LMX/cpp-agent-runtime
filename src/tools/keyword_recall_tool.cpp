#include "tools/keyword_recall_tool.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <vector>

#include "agent/item_catalog.hpp"

namespace tools {

    namespace {

        // 从 last_query 中抽取 ASCII tokens（英文/数字片段）：
        // 中文场景下，这是一种粗糙但可控的"关键词提取"，
        // 例如 "我想看一些偏 C++ 工程的视频" -> ["C", "++"] 取决于 punct 处理；
        // 这里把 '+', '-', '_', 字母数字都视为 token 字符，能命中 "C++"。
        std::vector<std::string> extractAsciiTokens(const std::string& s) {
            std::vector<std::string> tokens;
            std::string current;
            auto is_token_char = [](unsigned char c) {
                return std::isalnum(c) || c == '+' || c == '-' || c == '_' || c == '#';
            };
            for (unsigned char ch : s) {
                if (is_token_char(ch)) {
                    current.push_back(static_cast<char>(ch));
                } else {
                    if (current.size() >= 2) tokens.push_back(current);
                    current.clear();
                }
            }
            if (current.size() >= 2) tokens.push_back(current);
            return tokens;
        }

    } // namespace

    std::string KeywordRecallTool::name() const {
        return "keyword_recall";
    }

    agent::ToolResult KeywordRecallTool::execute(
        const nlohmann::json& input,
        agent::ToolContext& context
    ) {
        (void) input;

        const auto preference =
            context.preference_manager.getPreferenceJson(context.session_id);

        const std::string last_query =
            preference.value("last_query", std::string{});

        std::unordered_set<std::string> positive_tags;
        if (preference.contains("positive_tags")) {
            for (const auto& t : preference["positive_tags"]) {
                positive_tags.insert(t.get<std::string>());
            }
        }

        // 阶段 1：从 last_query 中抽 ASCII tokens（如 "C++"、"Redis"）
        const auto query_tokens = extractAsciiTokens(last_query);

        // 阶段 2：对每个物料：
        //   - 标签精确命中：+1 / 个
        //   - title 包含 query token：+1 / 个
        nlohmann::json candidates = nlohmann::json::array();

        for (const auto& item : agent::getItemCatalog()) {
            int tag_hits = 0;
            for (const auto& tag : item.tags) {
                if (positive_tags.count(tag) > 0) {
                    ++tag_hits;
                }
            }

            int title_hits = 0;
            for (const auto& tok : query_tokens) {
                if (item.title.find(tok) != std::string::npos) {
                    ++title_hits;
                }
            }

            const int total = tag_hits + title_hits;
            if (total == 0) continue;

            // 简易归一：每命中一项 0.3 分，封顶 1.0
            const double score = std::min(1.0, total * 0.3);

            candidates.push_back({
                {"id", item.id},
                {"title", item.title},
                {"tags", item.tags},
                {"category", item.category},
                {"score", score},
                {"sources", nlohmann::json::array({"keyword"})}
            });
        }

        std::sort(candidates.begin(), candidates.end(),
                  [](const nlohmann::json& a, const nlohmann::json& b) {
                      return a["score"].get<double>() > b["score"].get<double>();
                  });

        return agent::ToolResult::ok({
            {"candidates", candidates},
            {"source", "keyword"}
        });
    }

} // namespace tools
