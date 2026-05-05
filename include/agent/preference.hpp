#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace agent {

    struct Preference {
        std::vector<std::string> positive_tags;
        std::vector<std::string> negative_tags;
        std::vector<std::string> preferred_categories;
        std::vector<std::string> rejected_categories;

        std::string last_query;
        int turn_count{0};  // 表示当前 session 已经进行了多少轮交互（对话轮数），暂时没用到
    };

    nlohmann::json toJson(const Preference& preference);

} // namespace agent