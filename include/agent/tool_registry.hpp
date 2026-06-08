#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "agent/tool.hpp"

namespace agent {

class ToolRegistry {
public:
    void registerTool(std::unique_ptr<Tool> tool);

    Tool* getTool(const std::string& name) const;

    bool hasTool(const std::string& name) const;

    std::vector<std::string> listTools() const;

private:
    std::unordered_map<std::string, std::unique_ptr<Tool>> tools_;
};

} // namespace agent
