#include "agent/item_catalog.hpp"

namespace agent {

    // 物料 tags / category 使用 TagNormalizer 归一化后的标识符（英文），
    // 以便与 PreferenceManager 中存储的偏好直接做精确匹配。
    // title 保留中文，供展示与 keyword_recall 的子串匹配使用。
    const std::vector<CatalogItem>& getItemCatalog() {
        static const std::vector<CatalogItem> catalog = {
            {"v_001", "深入理解 C++ 后端架构",
             {"cpp_backend", "engineering_practice", "system_design"},
             "video", 0.90},

            {"v_002", "Linux 内核源码导读课",
             {"linux", "kernel", "course_project"},
             "course", 0.70},

            {"v_003", "Redis 真实生产事故复盘",
             {"redis", "engineering_practice", "system_design"},
             "video", 0.85},

            {"v_004", "C++ 入门十讲",
             {"cpp_backend", "beginner", "course_project"},
             "course", 0.60},

            {"v_005", "大型推荐系统工程实战",
             {"recommendation_system", "engineering_practice"},
             "video", 0.95},

            {"v_006", "Go 后端踩坑记录",
             {"go_backend", "engineering_practice"},
             "blog", 0.50},

            {"v_007", "数据库索引原理",
             {"database", "system_design"},
             "video", 0.65},

            {"v_008", "C++ 模板元编程从入门到放弃",
             {"cpp_backend", "beginner"},
             "blog", 0.40},

            {"v_009", "分布式存储系统设计",
             {"distributed", "system_design", "engineering_practice"},
             "video", 0.80},

            {"v_010", "Python 爬虫课程项目",
             {"python", "course_project", "beginner"},
             "course", 0.50},
        };
        return catalog;
    }

} // namespace agent
