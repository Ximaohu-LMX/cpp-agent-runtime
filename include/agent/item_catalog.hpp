#pragma once

#include <string>
#include <vector>

namespace agent {

    /**
     * @brief 物料库中单个候选项的静态属性
     *        三路召回（向量/关键词/热度）共享同一份物料，
     *        仅在打分逻辑上区分。
     */
    struct CatalogItem {
        std::string id;
        std::string title;
        std::vector<std::string> tags;
        std::string category;
        double popularity{0.0};   // 用于热度召回
    };

    /**
     * @help 返回硬编码的物料库（推荐链路骨架阶段使用）。
     *       后续接入真实索引/数据库时可替换为按需加载实现。
     * @return 全部候选项的只读引用，进程内单例
     */
    const std::vector<CatalogItem>& getItemCatalog();

} // namespace agent
