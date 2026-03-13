// ============================================================================
// tool_executor.hpp - 工具执行器（Step 9 注册表版）
// ============================================================================
// 演进：使用注册表替代硬编码分发
// - 不再依赖具体工具类
// - 通过注册表查找工具
// - 支持动态扩展
// ============================================================================

#pragma once

#include "tool.hpp"
#include "tool_registry.hpp"
#include <iostream>

class ToolExecutor {
public:
    // 执行工具调用（同步）
    static ToolResult execute_sync(const ToolCall& call) {
        std::cout << "[⚙️ 执行工具] " << call.name << std::endl;
        
        // 从注册表获取工具
        auto tool = ToolRegistry::instance().get_tool(call.name);
        
        if (!tool) {
            return ToolResult::fail(
                "未知工具: " + call.name + "\n" +
                "可用工具: " + get_available_tools()
            );
        }
        
        // 通过虚函数调用，多态执行
        return tool->execute(call.arguments);
    }
    
    // 获取可用工具列表
    static std::string get_available_tools() {
        auto names = ToolRegistry::instance().get_tool_names();
        std::string result;
        for (size_t i = 0; i < names.size(); ++i) {
            if (i > 0) result += ", ";
            result += names[i];
        }
        return result;
    }
    
    // 检查工具是否存在
    static bool has_tool(const std::string& name) {
        return ToolRegistry::instance().has_tool(name);
    }
};
