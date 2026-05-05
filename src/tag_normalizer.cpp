#include "agent/tag_normalizer.hpp"
#include <algorithm>

namespace agent {

namespace {

/**
 * 向 vector 中添加一个不重复的非空字符串。
 * 用于标准化后去重：多个别名可能映射到同一个标准名，需要避免重复写入。
 */
void addUnique(std::vector<std::string>& values, const std::string& value) {
    if (value.empty()) {
        return;
    }
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

} // namespace

/**
 * 构造函数：初始化标签和类别的别名映射表。
 *
 * 映射规则：左侧为 LLM 可能输出的原始文本，右侧为系统内部统一的标准标识符。
 * 同一个标准标识符可以有多个别名入口（如 "C++" 和 "cpp" 都映射到 "cpp"）。
 *
 * 当前为硬编码，后续可改为从 JSON 配置文件加载。
 */
TagNormalizer::TagNormalizer() {
    // ---- 标签别名映射 ----
    // 覆盖编程语言、技术方向、内容类型等维度
    tag_alias_ = {
        {"C++", "cpp"},
        {"cpp", "cpp"},
        {"C++后端", "cpp_backend"},
        {"后端", "backend"},
        {"工程实践", "engineering_practice"},
        {"真实工程", "engineering_practice"},       // "真实工程" 是 "工程实践" 的口语化别名
        {"系统设计", "system_design"},
        {"数据库", "database"},
        {"Agent", "agent"},
        {"推荐系统", "recommendation_system"},
        {"课程", "course"},
        {"课程项目", "course_project"},
        {"入门", "beginner"},
        {"基础", "beginner"}                        // "基础" 视为 "入门" 的同义词
    };

    // ---- 类别别名映射 ----
    // 类别粒度比标签更粗，用于内容分类过滤
    category_alias_ = {
        {"视频", "video"},
        {"video", "video"},
        {"工程实践", "engineering_practice"},
        {"真实工程", "engineering_practice"},
        {"课程", "course"},
        {"课程项目", "course_project"},
        {"入门", "beginner"},
        {"基础", "beginner"}
    };
}

/**
 * 对完整的 preference_delta JSON 对象进行标准化处理。
 *
 * 处理流程：
 *   1. 校验输入是否为 JSON object
 *   2. 对 positive_tags / negative_tags 逐一做标签标准化（别名 → 标准名）
 *   3. 对 preferred_categories / rejected_categories 逐一做类别标准化
 *   4. 透传 last_query 字段（不做标准化，供下游召回使用）
 *
 * 标准化过程中自动去重：若多个原始别名映射到同一标准名，结果中只保留一份。
 */
nlohmann::json TagNormalizer::normalizePreferenceDelta(
    const nlohmann::json& raw_delta
) const {
    if (!raw_delta.is_object()) {
        throw std::runtime_error("preference_delta must be a JSON object");
    }

    nlohmann::json result;

    // 标准化正向/负向标签
    if (raw_delta.contains("positive_tags")) {
        result["positive_tags"] =
            normalizeArray(raw_delta["positive_tags"], /*is_category=*/false);
    }
    if (raw_delta.contains("negative_tags")) {
        result["negative_tags"] =
            normalizeArray(raw_delta["negative_tags"], /*is_category=*/false);
    }

    // 标准化偏好/拒绝类别
    if (raw_delta.contains("preferred_categories")) {
        result["preferred_categories"] =
            normalizeArray(raw_delta["preferred_categories"], /*is_category=*/true);
    }
    if (raw_delta.contains("rejected_categories")) {
        result["rejected_categories"] =
            normalizeArray(raw_delta["rejected_categories"], /*is_category=*/true);
    }

    // last_query 原样透传，不经过标准化
    if (
        raw_delta.contains("last_query") &&
        raw_delta["last_query"].is_string()
    ) {
        result["last_query"] = raw_delta["last_query"];
    }

    return result;
}

/**
 * 单个标签的标准化：在 tag_alias_ 中查找映射。
 * 命中则返回标准名，未命中则原样返回（宽容策略，避免丢失 LLM 新输出的未知标签）。
 */
std::string TagNormalizer::normalizeTag(const std::string& tag) const {
    auto it = tag_alias_.find(tag);
    if (it != tag_alias_.end()) {
        return it->second;
    }
    // 未命中别名表，保留原值；后续可改为过滤或报警
    return tag;
}

/**
 * 单个类别的标准化：逻辑同 normalizeTag，使用 category_alias_ 映射表。
 */
std::string TagNormalizer::normalizeCategory(const std::string& category) const {
    auto it = category_alias_.find(category);
    if (it != category_alias_.end()) {
        return it->second;
    }
    return category;
}

/**
 * 对 JSON 字符串数组逐元素标准化，并去重。
 *
 * @param arr          JSON 数组，每个元素必须是字符串
 * @param is_category  true → 使用类别映射表；false → 使用标签映射表
 * @return 标准化 + 去重后的字符串列表
 * @throws std::runtime_error  输入不是数组，或数组元素不是字符串
 */
std::vector<std::string> TagNormalizer::normalizeArray(
    const nlohmann::json& arr,
    bool is_category
) const {
    if (!arr.is_array()) {
        throw std::runtime_error("preference field must be an array");
    }

    std::vector<std::string> result;
    for (const auto& item : arr) {
        if (!item.is_string()) {
            throw std::runtime_error("preference array item must be a string");
        }
        const std::string raw = item.get<std::string>();
        const std::string normalized =
            is_category ? normalizeCategory(raw) : normalizeTag(raw);
        addUnique(result, normalized);  // 多个别名可能映射到同一标准名，去重
    }
    return result;
}

} // namespace agent