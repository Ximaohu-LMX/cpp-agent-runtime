#include "agent/tool_registry.hpp"

#include <stdexcept>

namespace agent {

void ToolRegistry::registerTool(std::shared_ptr<Tool> tool) {
    if (!tool) {
        throw std::invalid_argument("cannot register null tool");
    }

    const std::string tool_name = tool->name();
    if (tool_name.empty()) {
        throw std::invalid_argument("tool name cannot be empty");
    }

    std::lock_guard<std::mutex> lock(mutex_);

    if (tools_.count(tool_name) > 0) {
        throw std::runtime_error("tool already registered: " + tool_name);
    }

    tools_[tool_name] = std::move(tool);
}

std::shared_ptr<Tool> ToolRegistry::getTool(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = tools_.find(name);
    if (it == tools_.end()) {
        return nullptr;
    }

    return it->second;
}

bool ToolRegistry::hasTool(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tools_.count(name) > 0;
}

std::vector<std::string> ToolRegistry::listTools() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> names;
    names.reserve(tools_.size());

    for (const auto& [name, _] : tools_) {
        names.push_back(name);
    }

    return names;
}

} // namespace agent
