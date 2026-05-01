#include "agent/session_manager.hpp"

namespace agent {

void SessionManager::createSessionIfNotExists(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    ensureSessionUnlocked(session_id);
}

// json中 "memory" 放的是 setValue/getValue 读写的数据，"events" 放的是 appendEvent 追加的执行事件。

void SessionManager::setValue(
    const std::string& session_id,
    const std::string& key,
    const nlohmann::json& value
) {
    std::lock_guard<std::mutex> lock(mutex_);
    ensureSessionUnlocked(session_id);
    sessions_[session_id]["memory"][key] = value;
}

std::optional<nlohmann::json> SessionManager::getValue(
    const std::string& session_id,
    const std::string& key
) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto session_it = sessions_.find(session_id);
    if (session_it == sessions_.end()) {
        return std::nullopt;
    }

    const auto& session = session_it->second;
    if (!session.contains("memory") || !session["memory"].contains(key)) {
        return std::nullopt;
    }

    return session["memory"][key];
}

void SessionManager::appendEvent(
    const std::string& session_id,
    const nlohmann::json& event
) {
    std::lock_guard<std::mutex> lock(mutex_);
    ensureSessionUnlocked(session_id);
    sessions_[session_id]["events"].push_back(event);
}

nlohmann::json SessionManager::dumpSession(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return nlohmann::json::object();
    }

    return it->second;
}

void SessionManager::ensureSessionUnlocked(const std::string& session_id) {
    auto& session = sessions_[session_id];

    if (!session.is_object()) {
        session = nlohmann::json::object();
    }

    if (!session.contains("session_id")) {
        session["session_id"] = session_id;
    }

    if (!session.contains("memory")) {
        session["memory"] = nlohmann::json::object();
    }

    if (!session.contains("events")) {
        session["events"] = nlohmann::json::array();
    }
}

} // namespace agent
