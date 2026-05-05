#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

#include "agent/preference.hpp"

namespace agent {

    /**
     * @brief 会话级用户偏好的中心管理器
     *       职责：
     *         1. 以 session_id 为键，维护每个会话独立的 Preference 状态
     *         2. 接收来自工具（如 PreferenceUpdateTool）的偏好增量并合并
     *         3. 对外提供 JSON 形式的偏好快照，供下游推荐 / 检索工具使用
     *       线程安全：内部通过 mutex_ 加锁，所有 public 方法均可在多线程下调用
     */
    class PreferenceManager {
    public:
        /**
         * @help 若给定 session_id 还没有对应的 Preference 记录则创建一个空记录；
         *       已存在则保持原样不动。典型用途：会话开始时预先初始化，
         *       避免后续读取返回默认空值。
         * @param session_id 会话唯一标识，通常由 Plan.session_id 透传而来
         */
        void createSessionIfNotExists(const std::string& session_id);

        /**
         * @help 将一次偏好增量合并到指定会话的当前偏好中（核心方法）。
         *       合并语义：
         *         - 数组字段（positive_tags / negative_tags /
         *           preferred_categories / rejected_categories）做去重并集合并
         *         - last_query 字段做覆盖更新
         *         - 负反馈优先：同一项同时出现在正向与负向中时，从正向集合中删除，避免推荐冲突
         *         - 每次调用 turn_count 自增 1
         *       若 session_id 不存在会自动创建空记录后再合并。
         * @param session_id       会话唯一标识
         * @param preference_delta 偏好增量 JSON，可包含以下可选字段：
         *                           - positive_tags / negative_tags：字符串数组
         *                           - preferred_categories / rejected_categories：字符串数组
         *                           - last_query：字符串
         */
        void updatePreference(
            const std::string& session_id,
            const nlohmann::json& preference_delta
        );

        /**
         * @help 读取指定会话的当前偏好快照（只读操作）。
         *       若会话不存在则返回一个默认空 Preference 对应的 JSON，
         *       从而免去调用方的空指针/缺失分支判断。
         *       本方法为 const，但内部仍会加锁（mutex_ 声明为 mutable）。
         * @param session_id 会话唯一标识
         * @return 该会话当前 Preference 的 JSON 表示
         */
        nlohmann::json getPreferenceJson(const std::string& session_id) const;

        /**
         * @help 将指定会话的偏好重置为初始空状态。
         *       用途：会话结束、用户主动“清空喜好”、或调试期间复位。
         * @param session_id 会话唯一标识
         */
        void clearPreference(const std::string& session_id);

    private:
        /**
         * @help 内部辅助方法：确保 session_id 在 preferences_ 中存在对应记录。
         *       命名后缀 Unlocked 约定：调用方必须已经持有 mutex_，
         *       本函数不再加锁，以避免在已加锁的 public 方法内再次加锁导致死锁。
         * @param session_id 会话唯一标识
         */
        void ensureSessionUnlocked(const std::string& session_id);

    private:
        /// 保护 preferences_ 的互斥量；声明为 mutable 以便 const 方法也可加锁
        mutable std::mutex mutex_;

        /// session_id -> Preference 映射，存放每个会话的累计偏好状态
        std::unordered_map<std::string, Preference> preferences_;
    };

} // namespace agent
