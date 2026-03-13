// ============================================================================
// registry.hpp - 工具注册表
// ============================================================================

#pragma once

#include "tool.hpp"
#include <map>
#include <memory>

class ToolRegistry {
public:
    using ToolPtr = std::shared_ptr<Tool>;
    
    void register_tool(ToolPtr tool) {
        tools_[tool->get_name()] = tool;
    }
    
    ToolPtr get_tool(const std::string& name) {
        auto it = tools_.find(name);
        return (it != tools_.end()) ? it->second : nullptr;
    }
    
    json::array list_tools() const {
        json::array arr;
        for (const auto& [name, tool] : tools_) {
            json::object info;
            info["name"] = name;
            info["description"] = tool->get_description();
            arr.push_back(info);
        }
        return arr;
    }

private:
    std::map<std::string, ToolPtr> tools_;
};
