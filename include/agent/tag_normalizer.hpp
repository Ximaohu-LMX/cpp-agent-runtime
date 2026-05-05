#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

namespace agent {

    /**
     * TagNormalizer —— 标签与类别的标准化器
     *
     * 负责将 LLM 输出的偏好中可能出现的别名、同义词、非标准写法
     * 映射到系统内部统一的标准标签/类别名称。
     *
     * 例如："cpp" / "c++" / "C++" 统一映射为 "C++"，
     *       "后端" / "backend" 统一映射为 "后端开发"。
     *
     * 当前实现基于硬编码的别名映射表（tag_alias_ / category_alias_），
     * 接口设计上可后续替换为配置文件加载或 LLM 动态标准化。
     */
    class TagNormalizer {
    public:
        /// 构造函数，初始化内置的别名映射表
        TagNormalizer();

        /**
         * 对一个完整的 preference_delta JSON 进行标准化
         *
         * 输入格式示例：
         * {
         *   "positive_tags": ["cpp", "系统设计"],
         *   "negative_tags": ["课程项目"],
         *   "positive_categories": ["backend"],
         *   "negative_categories": []
         * }
         *
         * 会对其中每个数组的每个元素调用 normalizeTag 或 normalizeCategory，
         * 返回标准化后的同结构 JSON。
         */
        nlohmann::json normalizePreferenceDelta(
            const nlohmann::json& raw_delta
        ) const;

    private:
        /// 将单个标签映射为标准名称，查不到则原样返回
        std::string normalizeTag(const std::string& tag) const;

        /// 将单个类别映射为标准名称，查不到则原样返回
        std::string normalizeCategory(const std::string& category) const;

        /**
         * 对 JSON 数组中的每个字符串元素逐一标准化
         * @param arr        JSON 字符串数组
         * @param is_category true 时走 category 映射，false 时走 tag 映射
         * @return 标准化后的字符串列表（已去重）
         */
        std::vector<std::string> normalizeArray(
            const nlohmann::json& arr,
            bool is_category
        ) const;

    private:
        std::unordered_map<std::string, std::string> tag_alias_;      ///< 标签别名 -> 标准标签
        std::unordered_map<std::string, std::string> category_alias_; ///< 类别别名 -> 标准类别
    };

} // namespace agent