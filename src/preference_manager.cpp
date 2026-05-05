#include "agent/preference_manager.hpp"

#include <algorithm>

namespace agent {

namespace {

// 向 vector 中追加一个字符串，若为空或已存在则跳过，保证元素唯一
void addUnique(std::vector<std::string>& values, const std::string& value) {
    if (value.empty()) {
        return;
    }
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

// 从 JSON 数组中提取字符串元素，逐个去重追加到目标 vector
void addUniqueFromJsonArray(
    std::vector<std::string>& values,
    const nlohmann::json& arr
) {
    if (!arr.is_array()) {
        return;
    }

    for (const auto& item : arr) {
        if (item.is_string()) {
            addUnique(values, item.get<std::string>());
        }
    }
}

// 从 target 中移除所有出现在 to_remove 中的元素（用于实现负向覆盖正向）
void removeValues(
    std::vector<std::string>& target,
    const std::vector<std::string>& to_remove
) {
    target.erase(
        std::remove_if(
            target.begin(),
            target.end(),
            [&](const std::string& value) {
                return std::find(to_remove.begin(), to_remove.end(), value) != to_remove.end();
            }
        ),
        target.end()
    );
}

} // namespace

// 将 Preference 结构体序列化为 JSON，便于工具输出和外部消费
nlohmann::json toJson(const Preference& preference) {
    return {
        {"positive_tags", preference.positive_tags},
        {"negative_tags", preference.negative_tags},
        {"preferred_categories", preference.preferred_categories},
        {"rejected_categories", preference.rejected_categories},
        {"last_query", preference.last_query},
        {"turn_count", preference.turn_count}
    };
}

// 对外接口：若 session 不存在则创建一个空的 Preference 记录
void PreferenceManager::createSessionIfNotExists(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    ensureSessionUnlocked(session_id);
}

// 核心方法：将一次偏好增量（preference_delta）合并到当前 session 的偏好中
// 成员变量： std::unordered_map<std::string, Preference> preferences_;
void PreferenceManager::updatePreference(
    const std::string& session_id,
    const nlohmann::json& preference_delta
) {
    // 加锁保证多线程下的合并安全
    std::lock_guard<std::mutex> lock(mutex_);
    ensureSessionUnlocked(session_id);

    auto& pref = preferences_[session_id];

    // 阶段 1：合并正向偏好（喜欢的 tag 与品类）
    if (preference_delta.contains("positive_tags")) {
        addUniqueFromJsonArray(pref.positive_tags, preference_delta["positive_tags"]);
    }

    if (preference_delta.contains("preferred_categories")) {
        addUniqueFromJsonArray(pref.preferred_categories, preference_delta["preferred_categories"]);
    }

    // 阶段 2：合并负向偏好（讨厌的 tag 与品类）
    if (preference_delta.contains("negative_tags")) {
        addUniqueFromJsonArray(pref.negative_tags, preference_delta["negative_tags"]);
    }

    if (preference_delta.contains("rejected_categories")) {
        addUniqueFromJsonArray(pref.rejected_categories, preference_delta["rejected_categories"]);
    }

    // 阶段 3：更新最近一次的查询语句，便于后续轮次参考上下文
    if (
        preference_delta.contains("last_query") &&
        preference_delta["last_query"].is_string()
    ) {
        pref.last_query = preference_delta["last_query"].get<std::string>();
    }

    // 阶段 4：负反馈优先策略
    // 若同一个 tag/category 同时出现在正向和负向中，以负向为准，
    // 即从正向集合中删除掉所有命中负向的项目，避免推荐冲突。
    removeValues(pref.positive_tags, pref.negative_tags);
    removeValues(pref.preferred_categories, pref.rejected_categories);

    // 累加交互轮次计数
    pref.turn_count++;
}

// 查询接口：返回指定 session 的当前偏好快照（JSON 格式）
nlohmann::json PreferenceManager::getPreferenceJson(
    const std::string& session_id
) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = preferences_.find(session_id);
    if (it == preferences_.end()) {
        // session 不存在时返回一个默认的空偏好，避免上游再做空指针判断
        return toJson(Preference{});
    }

    return toJson(it->second);
}

// 重置某个 session 的偏好为初始状态（常用于会话结束或调试）
void PreferenceManager::clearPreference(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    preferences_[session_id] = Preference{};
}

// 内部辅助：在已持有 mutex_ 的前提下确保 session 记录存在
// 命名后缀 Unlocked 表示调用者已加锁，本函数不再重复加锁
void PreferenceManager::ensureSessionUnlocked(const std::string& session_id) {
    if (preferences_.find(session_id) == preferences_.end()) {
        preferences_[session_id] = Preference{};
    }
}

} // namespace agent
