// ============================================================================
// registry.hpp - Tool 注册表
// ============================================================================

#pragma once

#include "tool.hpp"
#include <map>
#include <memory>
#include <vector>

// ----------------------------------------------------------------------------
// ToolRegistry 类
// ----------------------------------------------------------------------------
class ToolRegistry {
public:
    using ToolPtr = std::shared_ptr<Tool>;
    
    // 注册工具
    void register_tool(ToolPtr tool);
    
    // 获取工具
    ToolPtr get_tool(const std::string& name) const;
    
    // 列出所有工具名
    std::vector<std::string> list_tools() const;
    
    // 获取所有 Schema
    json::array get_all_schemas() const;
    
    // 执行工具
    json::value execute(const std::string& tool_name, const json::object& args) const;

private:
    std::map<std::string, ToolPtr> tools_;
    std::map<std::string, ToolSchema> schemas_;
    
    json::object schema_to_json(const ToolSchema& schema) const;
};
