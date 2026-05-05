#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace agent {

class SessionManager {
public:
    void createSessionIfNotExists(const std::string& session_id);

    void setValue(
        const std::string& session_id,
        const std::string& key,
        const nlohmann::json& value
    );

    std::optional<nlohmann::json> getValue(
        const std::string& session_id,
        const std::string& key  // 取某一步输出："output." + step_id
    ) const;

    void appendEvent(
        const std::string& session_id,
        const nlohmann::json& event
    );

    nlohmann::json dumpSession(const std::string& session_id) const;

private:
    void ensureSessionUnlocked(const std::string& session_id);

private:
    mutable std::mutex mutex_;
    // 哈希表，key 是 `session_id`（如 `"s1"`），value 是一个 JSON 对象，里面存着这个会话的所有共享数据。
    std::unordered_map<std::string, nlohmann::json> sessions_;

};

} // namespace agent
