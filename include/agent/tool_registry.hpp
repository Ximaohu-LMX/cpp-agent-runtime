#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "agent/tool.hpp"

namespace agent {

class ToolRegistry {
public:
    void registerTool(std::shared_ptr<Tool> tool);

    std::shared_ptr<Tool> getTool(const std::string& name) const;

    bool hasTool(const std::string& name) const;

    std::vector<std::string> listTools() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<Tool>> tools_;
};

} // namespace agent
