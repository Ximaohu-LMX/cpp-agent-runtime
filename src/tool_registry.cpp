#include "agent/tool_registry.hpp"

#include <stdexcept>
#include <utility>

namespace agent {

void ToolRegistry::registerTool(std::unique_ptr<Tool> tool) {
    if (!tool) {
        throw std::invalid_argument("cannot register null tool");
    }

    const std::string tool_name = tool->name();
    if (tool_name.empty()) {
        throw std::invalid_argument("tool name cannot be empty");
    }

    if (tools_.count(tool_name) > 0) {
        throw std::runtime_error("tool already registered: " + tool_name);
    }

    tools_.emplace(tool_name, std::move(tool));
}

Tool* ToolRegistry::getTool(const std::string& name) const {
    auto it = tools_.find(name);
    if (it == tools_.end()) {
        return nullptr;
    }

    return it->second.get();
}

bool ToolRegistry::hasTool(const std::string& name) const {
    return tools_.count(name) > 0;
}

std::vector<std::string> ToolRegistry::listTools() const {
    std::vector<std::string> names;
    names.reserve(tools_.size());

    for (const auto& [name, _] : tools_) {
        names.push_back(name);
    }

    return names;
}

} // namespace agent
