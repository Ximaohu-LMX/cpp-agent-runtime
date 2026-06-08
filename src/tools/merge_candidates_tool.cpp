#include "tools/merge_candidates_tool.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace tools {

    std::string MergeCandidatesTool::name() const {
        return "merge_candidates";
    }

    agent::ToolResult MergeCandidatesTool::execute(
        const nlohmann::json& input,
        agent::ToolContext& context
    ) {
        // 阶段 1：参数校验
        if (!input.contains("candidates_from_steps") ||
            !input["candidates_from_steps"].is_array()) {
            return agent::ToolResult::fail(
                "merge_candidates requires input.candidates_from_steps (array of step ids)"
            );
        }

        // 阶段 2：从 session 里逐个读取上游 step 的输出
        // id -> 当前最佳条目（保留 max score，sources 累加）
        std::unordered_map<std::string, nlohmann::json> merged;

        for (const auto& step_id_json : input["candidates_from_steps"]) {
            if (!step_id_json.is_string()) {
                return agent::ToolResult::fail(
                    "candidates_from_steps must be array of strings"
                );
            }
            const std::string step_id = step_id_json.get<std::string>();

            const auto step_output = context.session_manager.getValue(
                context.session_id,
                "output." + step_id
            );

            if (!step_output.has_value()) {
                return agent::ToolResult::fail(
                    "no output found for step: " + step_id
                );
            }
            if (!step_output.value().contains("candidates") ||
                !step_output.value()["candidates"].is_array()) {
                return agent::ToolResult::fail(
                    "step output missing candidates array: " + step_id
                );
            }

            // 阶段 3：合并该路召回的所有候选
            for (const auto& cand : step_output.value()["candidates"]) {
                const std::string id = cand.value("id", std::string{});
                if (id.empty()) continue;

                // 收集 sources（保留每路召回信息）
                std::unordered_set<std::string> sources;
                if (cand.contains("sources") && cand["sources"].is_array()) {
                    for (const auto& s : cand["sources"]) {
                        sources.insert(s.get<std::string>());
                    }
                }

                auto it = merged.find(id);
                if (it == merged.end()) {
                    // 首次出现：直接放入
                    merged.emplace(id, cand);
                } else {
                    // 已存在：取 max(score)，合并 sources
                    auto& existing = it->second;

                    // 合并已有 sources
                    if (existing.contains("sources") &&
                        existing["sources"].is_array()) {
                        for (const auto& s : existing["sources"]) {
                            sources.insert(s.get<std::string>());
                        }
                    }

                    const double new_score = cand.value("score", 0.0);
                    const double old_score = existing.value("score", 0.0);
                    if (new_score > old_score) {
                        existing["score"] = new_score;
                    }

                    // 写回去重后的 sources（按字典序保证稳定）
                    std::vector<std::string> sorted_sources(
                        sources.begin(), sources.end());
                    std::sort(sorted_sources.begin(), sorted_sources.end());
                    existing["sources"] = sorted_sources;
                }

                // 同步把当前条目的 sources 写齐
                std::vector<std::string> sorted_sources(
                    sources.begin(), sources.end());
                std::sort(sorted_sources.begin(), sorted_sources.end());
                merged[id]["sources"] = sorted_sources;
            }
        }

        // 阶段 4：转 array 并按 score 降序
        nlohmann::json candidates = nlohmann::json::array();
        for (auto& [id, cand] : merged) {
            candidates.push_back(std::move(cand));
        }

        std::sort(candidates.begin(), candidates.end(),
                  [](const nlohmann::json& a, const nlohmann::json& b) {
                      return a["score"].get<double>() > b["score"].get<double>();
                  });

        return agent::ToolResult::ok({
            {"candidates", candidates},
            {"merged_count", candidates.size()}
        });
    }

} // namespace tools
