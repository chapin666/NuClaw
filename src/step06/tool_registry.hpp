// ============================================================================
// tool_registry.hpp - 工具注册表
// ============================================================================

#pragma once

#include "tool.hpp"
#include <map>
#include <memory>

// ----------------------------------------------------------------------------
// 工具注册表
// ----------------------------------------------------------------------------
class ToolRegistry {
public:
    using ToolPtr = std::shared_ptr<Tool>;
    
    void register_tool(ToolPtr tool);
    ToolPtr get_tool(const std::string& name);
    json::array get_all_schemas() const;
    json::value execute(const std::string& tool_name, const json::object& args);

private:
    std::map<std::string, ToolPtr> tools_;
    std::map<std::string, ToolSchema> schemas_;
    
    json::object schema_to_json(const ToolSchema& schema);
};
